// 初始化页面
function initializePage() {
    createNavigation();
    createSections();
    // 默认显示第一个分支
    showSection('trunk');
}

// 创建导航栏
function createNavigation() {
    const sidebar = document.getElementById('sidebar');
    Object.values(branchData).forEach(branch => {
        const navItem = document.createElement('div');
        navItem.className = 'nav-item';
        navItem.textContent = branch.title;
        navItem.onclick = () => showSection(branch.id);
        if (branch.id === 'trunk') {
            navItem.classList.add('active');
        }
        sidebar.appendChild(navItem);
    });
}

// 创建内容区域
function createSections() {
    const mainContent = document.getElementById('main-content');
    Object.values(branchData).forEach(branch => {
        const section = document.createElement('div');
        section.id = branch.id;
        section.className = 'section';
        if (branch.id === 'trunk') {
            section.classList.add('active');
        }

        const title = document.createElement('h2');
        title.className = 'section-title';
        title.textContent = branch.title;
        section.appendChild(title);

        branch.items.forEach(item => {
            const itemDiv = document.createElement('div');
            itemDiv.className = 'item';

            const label = document.createElement('span');
            label.className = 'label';
            label.textContent = item.label + ':';
            itemDiv.appendChild(label);

            if (item.type === 'copy') {
                const button = document.createElement('button');
                button.className = 'copy-btn';
                button.textContent = item.content;
                button.onclick = () => copyText(item.content);
                itemDiv.appendChild(button);
            } else if (item.type === 'file') {
                // 创建容器来包含文本和按钮
                const contentContainer = document.createElement('div');
                contentContainer.className = 'file-content-container';

                // 显示路径的文本元素
                const pathText = document.createElement('span');
                pathText.className = 'file-path';
                pathText.textContent = item.content;
                contentContainer.appendChild(pathText);

                // 按钮容器
                const buttonContainer = document.createElement('div');
                buttonContainer.className = 'file-buttons';

                // 浏览器打开按钮
                const browserButton = document.createElement('button');
                browserButton.className = 'file-action-btn browser-btn';
                browserButton.innerHTML = '浏览器打开';
                browserButton.onclick = () => openInBrowser(item.content);
                buttonContainer.appendChild(browserButton);

                // 复制路径按钮
                const copyPathButton = document.createElement('button');
                copyPathButton.className = 'file-action-btn copy-path-btn';
                copyPathButton.innerHTML = '复制路径';
                copyPathButton.onclick = () => copyText(item.content);
                buttonContainer.appendChild(copyPathButton);

                contentContainer.appendChild(buttonContainer);
                itemDiv.appendChild(contentContainer);
            }

            section.appendChild(itemDiv);
        });

        mainContent.appendChild(section);
    });
}

// 在浏览器中打开
function openInBrowser(path) {
    window.open(path, '_blank');
}

// 复制文本功能
function copyText(text) {
    navigator.clipboard.writeText(text).then(() => {
        const tooltip = document.getElementById('tooltip');
        tooltip.style.display = 'block';
        setTimeout(() => {
            tooltip.style.display = 'none';
        }, 1000);
    }).catch(err => {
        // 如果clipboard API不可用，使用传统方法
        const textArea = document.createElement('textarea');
        textArea.value = text;
        document.body.appendChild(textArea);
        textArea.select();
        try {
            document.execCommand('copy');
            const tooltip = document.getElementById('tooltip');
            tooltip.style.display = 'block';
            setTimeout(() => {
                tooltip.style.display = 'none';
            }, 1000);
        } catch (err) {
            console.error('复制失败:', err);
        }
        document.body.removeChild(textArea);
    });
}

// 切换分支显示
function showSection(sectionId) {
    // 隐藏所有sections
    document.querySelectorAll('.section').forEach(section => {
        section.classList.remove('active');
    });
    // 显示选中的section
    document.getElementById(sectionId).classList.add('active');
    
    // 更新导航栏激活状态
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    const currentNav = Array.from(document.querySelectorAll('.nav-item'))
        .find(item => item.textContent === branchData[sectionId].title);
    if (currentNav) {
        currentNav.classList.add('active');
    }
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', initializePage);