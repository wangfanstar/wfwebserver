const branchData = {  
    trunk: {  
        id: 'trunk',  
        title: '主线B75 TRUNK（V7R31）',  
        description: '主干开发分支，用于新特性开发',  
        items: [  
            {  
                label: '主线负责人',  
                type: 'copy',  
                content: 'test1@example.com test2@example.com'  
            },  
            {  
                label: '主线说明',  
                type: 'text',  
                content: 'TEST-X B75主线代码库，用于新特性开发'  
            },  
            {  
                label: 'DRV库地址',  
                type: 'copy',  
                content: 'http://test.example.com/TEST-X_CODE/V7R1_SPRING/trunk/DRV'  
            },  
            {  
                label: 'PUBLIC库地址',  
                type: 'copy',  
                content: 'http://test.example.com/public-code/V7R1_SPRING/trunk/PUBLIC'  
            },  
            {  
                label: '北京编译lib地址',  
                type: 'copy',  
                content: '-d /var/testlib/V7R1_SPRING/trunk/'  
            },  
            {  
                label: '杭州编译lib地址',  
                type: 'copy',  
                content: '-d /mnt/testlib/V7R1_SPRING/trunk/'  
            },  
            {  
                label: '文件服务器地址',  
                type: 'file',  
                content: 'file://test.example.com/testlib/V7R1_SPRING/trunk/'  
            },  
            {  
                label: '标题模板(普通)',  
                type: 'copy',  
                content: '【TEST产品Trunk】【模块 问题级别】'  
            },  
            {  
                label: '标题模板(BRAS)',  
                type: 'copy',  
                content: '【TEST产品Trunk】【BRAS领域】【模块 问题级别】'  
            },  
            {  
                label: '审批人',  
                type: 'copy',  
                content: 'test1@example.com'  
            }  
        ]  
    },  
    d086sp: {  
        id: 'd086sp',  
        title: '维护D086SP',  
        description: 'D086SP维护分支，用于问题修复',  
        items: [  
            {  
                label: 'DRV库地址',  
                type: 'copy',  
                content: 'http://test.example.com/TEST-X_CODE/V7R1_D086SP/trunk/DRV'  
            },  
            {  
                label: 'PUBLIC库地址',  
                type: 'copy',  
                content: 'http://test.example.com/public-code/V7R1_D086SP/trunk/PUBLIC'  
            },  
            {  
                label: '北京编译lib地址',  
                type: 'copy',  
                content: '-d /var/testlib/V7R1_D086SP/trunk/'  
            },  
            {  
                label: '杭州编译lib地址',  
                type: 'copy',  
                content: '-d /mnt/testlib/V7R1_D086SP/trunk/'  
            },  
            {  
                label: '文件服务器地址',  
                type: 'file',  
                content: 'file://test.example.com/testlib/V7R1_D086SP/trunk/'  
            },  
            {  
                label: '标题模板(普通)',  
                type: 'copy',  
                content: '【TEST产品D086SP维护分支】【模块 问题级别】'  
            },  
            {  
                label: '标题模板(BRAS)',  
                type: 'copy',  
                content: '【TEST产品D086SP维护分支】【BRAS领域】【模块 问题级别】'  
            },  
            {  
                label: '审批人',  
                type: 'copy',  
                content: 'test1@example.com test2@example.com'  
            }  
        ]  
    },  
    d099sp: {  
        id: 'd099sp',  
        title: '主线D099SP',  
        description: 'D099SP维护分支，用于问题修复',  
        items: [  
            {  
                label: 'DRV库地址',  
                type: 'copy',  
                content: 'http://test.example.com/TEST-X_CODE/V7R1_D099SP/trunk/DRV'  
            },  
            {  
                label: 'PUBLIC库地址',  
                type: 'copy',  
                content: 'http://test.example.com/public-code/V7R1_D099SP/trunk/PUBLIC'  
            },  
            {  
                label: '北京编译lib地址',  
                type: 'copy',  
                content: '-d /var/testlib/V7R1_D099SP/trunk/'  
            },  
            {  
                label: '杭州编译lib地址',  
                type: 'copy',  
                content: '-d /mnt/testlib/V7R1_D099SP/trunk/'  
            },  
            {  
                label: '文件服务器地址',  
                type: 'file',  
                content: 'file://test.example.com/testlib/V7R1_D099SP/trunk/'  
            },  
            {  
                label: '标题模板(普通)',  
                type: 'copy',  
                content: '【TEST产品D099SP维护分支】【模块 问题级别】'  
            },  
            {  
                label: '标题模板(BRAS)',  
                type: 'copy',  
                content: '【TEST产品D099SP维护分支】【BRAS领域】【模块 问题级别】'  
            },  
            {  
                label: '审批人',  
                type: 'copy',  
                content: 'test1@example.com test2@example.com'  
            }  
        ]  
    }  
};  

export default branchData;