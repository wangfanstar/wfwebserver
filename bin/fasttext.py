from flask import Flask, request, render_template, jsonify
import sqlite3
import fasttext
import os
import chromadb
import configparser
import logging
import traceback
import json
import re
import math
from datetime import datetime

# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# 读取配置文件
def read_config():
    try:
        config = configparser.ConfigParser()
        config.read('mindocsearch.conf')
        
        config_dict = {
            'backup_db_path': config.get('DEFAULT', 'backupdbpath'),
            'backup_md_path': config.get('DEFAULT', 'backupmdpath'),
            'mindoc_server_url': config.get('DEFAULT', 'mindocserverurl'),
            'link_prefix': config.get('DEFAULT', 'link_prefix'),
            'model_path': config.get('DEFAULT', 'modelpath')
        }
        
        # 检查配置文件路径是否存在
        for key, path in config_dict.items():
            if 'path' in key and not os.path.exists(path):
                logger.warning(f"警告: 配置的路径不存在: {key}={path}")
        
        return config_dict
    except Exception as e:
        logger.error(f"读取配置文件出错: {str(e)}")
        raise

# 处理文本，移除换行符和多余空格，为 FastText 处理做准备
def preprocess_text(text):
    if not text:
        return ""
    # 将换行符替换为空格
    text = re.sub(r'\n+', ' ', text)
    # 删除多余空格
    text = re.sub(r'\s+', ' ', text)
    # 删除任何不可打印的字符
    text = re.sub(r'[^\x20-\x7E\u4e00-\u9fff]+', '', text)
    return text.strip()

# 处理 Markdown 文档，移除语法标记，提取纯文本内容
def process_markdown(text):
    if not text:
        return ""
    
    # 移除标题标记 (# 标题)
    text = re.sub(r'#+\s+', '', text)
    
    # 移除粗体和斜体 (**粗体**, *斜体*)
    text = re.sub(r'\*\*(.+?)\*\*', r'\1', text)
    text = re.sub(r'\*(.+?)\*', r'\1', text)
    
    # 移除行内代码 (`代码`)
    text = re.sub(r'`(.+?)`', r'\1', text)
    
    # 移除链接，只保留文本 ([文本](链接))
    text = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', text)
    
    # 移除代码块
    text = re.sub(r'```[\s\S]*?```', '', text)
    
    # 移除表格语法
    text = re.sub(r'\|.*\|', '', text)
    text = re.sub(r'[-:]+', '', text)
    
    # 移除HTML标签
    text = re.sub(r'<.*?>', '', text)
    
    # 基本预处理
    return preprocess_text(text)

# 生成智能摘要，优先包含查询关键词的段落
def generate_smart_summary(text, keywords, max_length=150):
    if not text:
        return ""
    
    # 将文本分段
    paragraphs = re.split(r'\n+', text)
    paragraphs = [p.strip() for p in paragraphs if p.strip()]
    
    if not paragraphs:
        return ""
    
    # 计算每个段落包含的关键词数量
    para_scores = []
    for para in paragraphs:
        para_lower = para.lower()
        keyword_count = sum(1 for keyword in keywords if keyword in para_lower)
        para_scores.append((para, keyword_count))
    
    # 按关键词匹配数排序
    para_scores.sort(key=lambda x: x[1], reverse=True)
    
    # 取得最匹配的段落
    best_para = para_scores[0][0] if para_scores else paragraphs[0]
    
    # 如果最匹配段落太长，进行截断
    if len(best_para) > max_length:
        return best_para[:max_length] + "..."
    
    return best_para

# 提取文本中的关键词
def extract_keywords(text, max_keywords=10):
    if not text:
        return []
    
    # 简单的高频词提取
    # 在实际应用中可以使用更复杂的方法，如TF-IDF或中文分词
    words = text.lower().split()
    
    # 移除常见的停用词
    stop_words = {'的', '了', '和', '是', '在', '我', '有', '为', '这', '也', 
                 '与', '中', '或', '从', '可以', '就是', '以', '及', '等', '被'}
    
    words = [w for w in words if w not in stop_words and len(w) > 1]
    
    # 计算词频
    word_freq = {}
    for word in words:
        word_freq[word] = word_freq.get(word, 0) + 1
    
    # 按频率排序并返回前N个词
    sorted_words = sorted(word_freq.items(), key=lambda x: x[1], reverse=True)
    return [w for w, _ in sorted_words[:max_keywords]]

# 全局配置
config = read_config()
logger.info(f"配置信息: {json.dumps(config, ensure_ascii=False, indent=2)}")
client = chromadb.PersistentClient(path="./chromadb")
model = None

# 初始化向量数据库
def init_vector_db():
    global model
    
    try:
        # 加载FastText模型
        model_path = config['model_path']
        logger.info(f"正在加载FastText模型: {model_path}")
        model = fasttext.load_model(model_path)
        
        # 检查是否已有集合
        collection_name = "mindoc_docs"
        
        # 获取所有集合名称 (ChromaDB v0.6.0+ 版本兼容性)
        collection_names = client.list_collections()
        logger.info(f"现有集合: {collection_names}")
        
        # 在v0.6.0中，collection_names直接是字符串列表
        collection_exists = collection_name in collection_names
        logger.info(f"集合 {collection_name} 是否存在: {collection_exists}")
        
        # 如果集合不存在，则创建并导入数据
        if not collection_exists:
            logger.info("创建新集合并导入数据")
            collection = client.create_collection(
                name=collection_name,
                metadata={"description": "MindDoc Markdown documents"}
            )
            import_documents(collection)
        else:
            logger.info(f"使用现有集合: {collection_name}")
            collection = client.get_collection(name=collection_name)
            
        # 检查集合中的文档数量
        doc_count = collection.count()
        logger.info(f"集合中的文档数量: {doc_count}")
        
        return collection
        
    except Exception as e:
        logger.error(f"初始化向量数据库出错: {str(e)}")
        logger.error(traceback.format_exc())
        raise

# 从SQLite导入文档到向量数据库
def import_documents(collection):
    try:
        # 检查数据库文件是否存在
        db_path = config['backup_db_path']
        if not os.path.exists(db_path):
            logger.error(f"数据库文件不存在: {db_path}")
            raise FileNotFoundError(f"找不到数据库文件: {db_path}")
            
        # 检查文件权限
        if not os.access(db_path, os.R_OK):
            logger.error(f"数据库文件无读取权限: {db_path}")
            raise PermissionError(f"无法读取数据库文件: {db_path}")
        
        # 连接SQLite数据库
        logger.info(f"连接数据库: {db_path}")
        conn = sqlite3.connect(db_path)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        
        # 查询获取文档和对应的book信息
        query = """
        SELECT d.document_id, d.document_name, d.identify as doc_identify, 
               d.markdown, b.identify as book_identify, b.book_name
        FROM md_documents d
        JOIN md_books b ON d.book_id = b.book_id
        WHERE d.markdown IS NOT NULL AND length(trim(d.markdown)) > 0
        """
        
        cursor.execute(query)
        documents = cursor.fetchall()
        conn.close()
        
        doc_count = len(documents)
        logger.info(f"从数据库加载了 {doc_count} 个文档")
        
        if doc_count == 0:
            logger.warning("警告: 数据库中没有找到有效的文档")
            return
            
        # 输出前5个文档示例
        for i, doc in enumerate(documents[:5]):
            logger.info(f"示例文档 {i+1}:")
            logger.info(f"  ID: {doc['document_id']}")
            logger.info(f"  标题: {doc['document_name']}")
            logger.info(f"  Markdown 长度: {len(doc['markdown'])}")
            logger.info(f"  Book Identify: {doc['book_identify']}")
            logger.info(f"  Doc Identify: {doc['doc_identify']}")
        
        # 批量处理，每批100个文档
        batch_size = 100
        total_added = 0
        
        for i in range(0, len(documents), batch_size):
            batch = documents[i:i+batch_size]
            
            ids = []
            docs = []
            metadatas = []
            embeddings = []
            
            for doc in batch:
                doc_id = f"doc_{doc['document_id']}"
                markdown_text = doc['markdown']
                
                # 如果markdown太短，可能没有意义，跳过
                if len(markdown_text.strip()) < 10:
                    continue
                
                # 预处理文本 - 移除换行符和处理Markdown语法
                processed_text = process_markdown(markdown_text)
                if not processed_text:
                    logger.warning(f"预处理后文本为空, 文档ID: {doc['document_id']}")
                    continue
                
                # 提取文档关键词，用于元数据
                doc_keywords = extract_keywords(processed_text)
                
                # 生成文档向量
                try:
                    doc_vector = model.get_sentence_vector(processed_text).tolist()
                except Exception as e:
                    logger.error(f"生成向量时出错, 文档ID: {doc['document_id']}, 错误: {str(e)}")
                    continue
                
                # 构建元数据
                metadata = {
                    'document_id': doc['document_id'],
                    'document_name': doc['document_name'],
                    'doc_identify': doc['doc_identify'],
                    'book_identify': doc['book_identify'],
                    'book_name': doc['book_name'],
                    'keywords': ','.join(doc_keywords),
                    'url': f"{config['link_prefix']}/{doc['book_identify']}/{doc['doc_identify']}"
                }
                
                ids.append(doc_id)
                docs.append(markdown_text)  # 存储原始文本，便于搜索结果展示
                metadatas.append(metadata)
                embeddings.append(doc_vector)
            
            if not ids:  # 如果这批次没有有效文档，跳过
                continue
                
            # 添加到向量数据库
            collection.add(
                ids=ids,
                documents=docs,
                metadatas=metadatas,
                embeddings=embeddings
            )
            
            total_added += len(ids)
            logger.info(f"已处理 {i+len(batch)}/{len(documents)} 个文档, 已添加 {total_added} 个文档")
        
        logger.info(f"导入完成，总共添加了 {total_added} 个文档到向量数据库")
        
    except sqlite3.OperationalError as e:
        logger.error(f"数据库操作错误: {str(e)}")
        print(f"数据库操作错误: {str(e)}")
        print(f"请检查数据库路径是否正确: {config['backup_db_path']}")
        raise
    except Exception as e:
        logger.error(f"处理文档时出错: {str(e)}")
        logger.error(traceback.format_exc())
        print(f"处理文档时出错: {str(e)}")
        raise

# 初始化向量数据库
try:
    collection = init_vector_db()
except Exception as e:
    logger.error(f"初始化失败: {str(e)}")
    
    # 检查备用集合是否已存在
    backup_collection_name = "mindoc_docs_empty"
    existing_collections = client.list_collections()
    
    if backup_collection_name in existing_collections:
        logger.warning(f"使用已有备用集合: {backup_collection_name}")
        collection = client.get_collection(name=backup_collection_name)
    else:
        # 创建一个空集合以便应用能启动
        logger.warning(f"创建新的备用集合: {backup_collection_name}")
        collection = client.create_collection(
            name=backup_collection_name,
            metadata={"description": "Empty collection due to initialization error"}
        )
    
    logger.warning("使用备用集合启动应用，搜索功能可能不可用")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/search', methods=['POST'])
def search():
    query = request.form.get('query', '')
    logger.info(f"收到搜索请求: {query}")
    
    if not query:
        return jsonify({'results': []})
    
    try:
        # 检查模型是否已加载
        if model is None:
            logger.error("错误: FastText模型未加载")
            return jsonify({
                'error': "系统错误: 模型未正确加载",
                'results': []
            })
        
        # 记录搜索开始时间
        start_time = datetime.now()
        
        # 预处理查询文本 - 移除换行符
        processed_query = preprocess_text(query)
        logger.info(f"预处理后的查询: {processed_query}")
        
        # 提取查询关键词
        query_keywords = set(processed_query.lower().split())
        logger.info(f"查询关键词: {query_keywords}")
        
        # 使用FastText模型将查询转换为向量
        query_vector = model.get_sentence_vector(processed_query).tolist()
        logger.info(f"查询向量已生成，维度: {len(query_vector)}")
        
        # 检查集合中的文档数量
        doc_count = collection.count()
        logger.info(f"向量数据库中的文档数量: {doc_count}")
        
        if doc_count == 0:
            logger.warning("向量数据库中没有文档，无法执行搜索")
            return jsonify({
                'warning': "向量数据库为空，请先导入文档",
                'results': []
            })
        
        # 从向量数据库中查询最相似的文档（获取更多结果用于后处理）
        results = collection.query(
            query_embeddings=[query_vector],
            n_results=min(200, doc_count),  # 获取更多结果便于筛选
            include=["documents", "metadatas", "distances"]
        )
        
        result_count = len(results['ids'][0]) if 'ids' in results and results['ids'] else 0
        logger.info(f"初始查询结果数量: {result_count}")
        
        # 检查结果是否为空
        if result_count == 0:
            logger.warning("搜索结果为空")
            return jsonify({
                'message': "没有找到相关文档",
                'results': []
            })
        
        # 构建搜索结果，并添加相似度增强
        search_results = []
        for i, (doc, metadata, distance) in enumerate(zip(
            results['documents'][0], 
            results['metadatas'][0],
            results['distances'][0]
        )):
            # 计算初始语义相似度分数（距离转换为相似度）
            base_similarity = 1 - distance/2
            
            # 关键词匹配分数增强
            # 从文档中提取文本用于关键词匹配
            doc_processed = process_markdown(doc).lower()
            doc_title = metadata['document_name'].lower()
            
            # 提取元数据中的关键词
            doc_keywords = metadata.get('keywords', '').lower().split(',')
            
            # 计算标题和内容中的关键词匹配度
            title_matches = sum(1 for keyword in query_keywords if keyword in doc_title)
            content_matches = sum(1 for keyword in query_keywords if keyword in doc_processed)
            keyword_matches = sum(1 for keyword in query_keywords if any(keyword in dk for dk in doc_keywords))
            
            # 根据关键词匹配增强相似度分数
            # 标题匹配权重更高
            keyword_boost = min(0.5, (
                title_matches * 0.3 + 
                content_matches * 0.05 + 
                keyword_matches * 0.1
            ) * (1.0 / max(len(query_keywords), 1)))
            
            # 最终相似度分数
            final_similarity = min(1.0, base_similarity + keyword_boost)
            
            # 只保留相似度超过阈值的结果
            min_similarity = 0.3  # 设置最低相似度阈值
            if final_similarity < min_similarity:
                continue
                
            # 生成文档摘要 (优先包含查询关键词的部分)
            summary = generate_smart_summary(doc, query_keywords, 200)
            
            # 如果关键词匹配不高，可能是错误匹配，降低排名
            rank_score = final_similarity
            if title_matches == 0 and content_matches <= 1:
                rank_score = final_similarity * 0.7
            
            search_results.append({
                'id': i + 1,
                'title': metadata['document_name'],
                'book_name': metadata['book_name'],
                'url': metadata['url'],
                'summary': summary,
                'similarity': round(final_similarity * 100),  # 转为百分比
                'base_similarity': round(base_similarity * 100),  # 调试用
                'keyword_boost': round(keyword_boost * 100),  # 调试用
                'title_matches': title_matches,  # 调试用
                'content_matches': content_matches,  # 调试用
                'rank_score': rank_score  # 用于排序
            })
        
        # 根据最终排名分数重新排序
        search_results.sort(key=lambda x: x['rank_score'], reverse=True)
        
        # 限制返回结果数量
        search_results = search_results[:100]
        
        # 重新编号
        for i, result in enumerate(search_results):
            result['id'] = i + 1
            
            # 移除内部排序用的字段
            if 'rank_score' in result:
                del result['rank_score']
        
        # 计算搜索耗时
        search_time = (datetime.now() - start_time).total_seconds()
        
        logger.info(f"返回过滤后的搜索结果: {len(search_results)} 条，耗时: {search_time:.3f}秒")
        return jsonify({
            'results': search_results,
            'count': len(search_results),
            'time': round(search_time, 3)
        })
        
    except Exception as e:
        logger.error(f"搜索过程中出错: {str(e)}")
        logger.error(traceback.format_exc())
        return jsonify({
            'error': f"搜索出错: {str(e)}",
            'results': []
        })

# 添加调试路由

@app.route('/debug/config')
def debug_config():
    """查看当前配置"""
    return jsonify({
        'config': config,
        'db_exists': os.path.exists(config['backup_db_path']),
        'model_exists': os.path.exists(config['model_path']),
        'chromadb_path': os.path.abspath('./chromadb')
    })

@app.route('/debug/db-info')
def debug_db_info():
    """查看数据库信息"""
    try:
        # 连接数据库
        conn = sqlite3.connect(config['backup_db_path'])
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()
        
        # 获取文档数量
        doc_count = cursor.execute("SELECT COUNT(*) FROM md_documents").fetchone()[0]
        markdown_count = cursor.execute("SELECT COUNT(*) FROM md_documents WHERE markdown IS NOT NULL AND length(trim(markdown)) > 0").fetchone()[0]
        
        # 获取书籍数量
        book_count = cursor.execute("SELECT COUNT(*) FROM md_books").fetchone()[0]
        
        # 获取示例文档
        sample_docs = cursor.execute("""
            SELECT d.document_id, d.document_name, d.identify, b.book_name, b.identify as book_identify, 
                   length(d.markdown) as markdown_length
            FROM md_documents d
            JOIN md_books b ON d.book_id = b.book_id
            WHERE d.markdown IS NOT NULL
            LIMIT 10
        """).fetchall()
        
        samples = []
        for doc in sample_docs:
            samples.append({
                'document_id': doc['document_id'],
                'document_name': doc['document_name'],
                'doc_identify': doc['identify'],
                'book_name': doc['book_name'],
                'book_identify': doc['book_identify'],
                'markdown_length': doc['markdown_length'],
                'url': f"{config['link_prefix']}/{doc['book_identify']}/{doc['identify']}"
            })
        
        conn.close()
        
        return jsonify({
            'database_path': config['backup_db_path'],
            'total_documents': doc_count,
            'documents_with_markdown': markdown_count,
            'total_books': book_count,
            'sample_documents': samples
        })
        
    except Exception as e:
        return jsonify({
            'error': str(e),
            'traceback': traceback.format_exc()
        })

@app.route('/debug/vector-db')
def debug_vector_db():
    """查看向量数据库信息"""
    try:
        # 获取所有集合名称 (ChromaDB v0.6.0+ 兼容)
        collections = client.list_collections()
        collection_info = []
        
        for coll_name in collections:
            try:
                # v0.6.0+ API 直接使用名称获取集合
                coll_obj = client.get_collection(name=coll_name)
                count = coll_obj.count()
                collection_info.append({
                    'name': coll_name,
                    'document_count': count,
                    'metadata': coll_obj.metadata
                })
            except Exception as e:
                collection_info.append({
                    'name': coll_name,
                    'error': str(e)
                })
        
        # 获取当前集合的一些示例
        samples = []
        try:
            if collection.count() > 0:
                peek_result = collection.peek(limit=5)
                
                for i, (doc_id, doc, metadata) in enumerate(zip(
                    peek_result['ids'],
                    peek_result.get('documents', [None] * len(peek_result['ids'])),
                    peek_result.get('metadatas', [None] * len(peek_result['ids']))
                )):
                    samples.append({
                        'id': doc_id,
                        'document_preview': doc[:100] + '...' if doc and len(doc) > 100 else doc,
                        'metadata': metadata
                    })
        except Exception as e:
            logger.error(f"获取示例数据出错: {str(e)}")
        
        return jsonify({
            'chromadb_path': os.path.abspath('./chromadb'),
            'collections': collection_info,
            'current_collection': {
                'name': collection.name,
                'document_count': collection.count(),
                'samples': samples
            }
        })
        
    except Exception as e:
        return jsonify({
            'error': str(e),
            'traceback': traceback.format_exc()
        })

# 添加重建索引功能
@app.route('/rebuild-index', methods=['POST'])
def rebuild_index():
    """重建向量索引"""
    try:
        # 删除现有集合
        collection_name = "mindoc_docs"
        collections = client.list_collections()
        # v0.6.0+ 兼容性处理
        collection_exists = collection_name in collections
        
        if collection_exists:
            client.delete_collection(collection_name)
            logger.info(f"已删除现有集合: {collection_name}")
        
        # 创建新集合
        new_collection = client.create_collection(
            name=collection_name,
            metadata={"description": "MindDoc Markdown documents"}
        )
        
        # 导入文档
        import_documents(new_collection)
        
        # 更新全局集合对象
        global collection
        collection = new_collection
        
        return jsonify({
            'success': True,
            'message': '向量索引重建成功',
            'document_count': collection.count()
        })
        
    except Exception as e:
        logger.error(f"重建索引出错: {str(e)}")
        logger.error(traceback.format_exc())
        return jsonify({
            'success': False,
            'error': str(e)
        })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)