const mainlineInfo = {
    title: '主线信息',
    items: [
        {
            label: '主线负责人',
            type: 'copy',
            content: 'yanglan@h3c.com zhangzongsheng.07463@h3c.com'
        },
        {
            label: '主线说明',
            type: 'text',
            content: 'SR8800-X B75主线代码库，用于新特性开发'
        }
    ]
};

const branchData = {
    trunk_b75: {
        id: 'trunk_b75',
        title: '主线B75 TRUNK（V7R31）',
        description: '主干开发分支，用于新特性开发',
        items: [
            {
                label: 'DRV库地址',
                type: 'copy',
                content: 'http://10.165.104.66/SR8800-X_CODE/V7R1_SPRINGB75/trunk/DRV'
            },
            {
                label: 'PUBLIC库地址',
                type: 'copy',
                content: 'http://10.153.120.104/cmwcode-public/V7R1_SPRINGB75/trunk/PUBLIC'
            },
            {
                label: '北京编译lib地址',
                type: 'copy',
                content: '-d /var/mntcmmold/V7R1_SPRINGB75/trunk/'
            },
            {
                label: '杭州编译lib地址',
                type: 'copy',
                content: '-d /mnt/cilib_old/V7R1_SPRINGB75/trunk/'
            },
            {
                label: '文件服务器地址',
                type: 'file',
                content: 'file://10.153.3.125/cilib-old/V7R1_SPRINGB75/trunk/'
            },
            {
                label: '标题模板(普通)',
                type: 'copy',
                content: '【SR88产品B75Trunk】【模块 问题级别】'
            },
            {
                label: '标题模板(BRAS)',
                type: 'copy',
                content: '【SR88产品B75Trunk】【BRAS领域】【模块 问题级别】'
            },
            {
                label: '审批人',
                type: 'copy',
                content: 'yanglan@h3c.com'
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
                content: 'http://10.165.104.66/SR8800-X_CODE/V7R1_B75D086SP/trunk/DRV'
            },
            {
                label: 'PUBLIC库地址',
                type: 'copy',
                content: 'http://10.153.120.104/cmwcode-public/V7R1_B75D086SP/trunk/PUBLIC'
            },
            {
                label: '北京编译lib地址',
                type: 'copy',
                content: '-d /var/mntcmmold/V7R1_B75D086SP/trunk/'
            },
            {
                label: '杭州编译lib地址',
                type: 'copy',
                content: '-d /mnt/cilib_old/V7R1_B75D086SP/trunk/'
            },
            {
                label: '文件服务器地址',
                type: 'file',
                content: 'file://10.153.3.125/cilib-old/V7R1_B75D086SP/trunk/'
            },
            {
                label: '标题模板(普通)',
                type: 'copy',
                content: '【SR88B75D086SP维护分支】【模块 问题级别】'
            },
            {
                label: '标题模板(BRAS)',
                type: 'copy',
                content: '【SR88B75D086SP维护分支】【BRAS领域】【模块 问题级别】'
            },
            {
                label: '审批人',
                type: 'copy',
                content: 'yanglan@h3c.com zhangzongsheng.07463@h3c.com'
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
                content: 'http://10.165.104.66/SR8800-X_CODE/V7R1_B75D099SP/trunk/DRV'
            },
            {
                label: 'PUBLIC库地址',
                type: 'copy',
                content: 'http://10.153.120.104/cmwcode-public/V7R1_B75D099SP/trunk/PUBLIC'
            },
            {
                label: '北京编译lib地址',
                type: 'copy',
                content: '-d /var/mntcmmold/V7R1_B75D099SP/trunk/'
            },
            {
                label: '杭州编译lib地址',
                type: 'copy',
                content: '-d /mnt/cilib_old/V7R1_B75D099SP/trunk/'
            },
            {
                label: '文件服务器地址',
                type: 'file',
                content: 'file://10.153.3.125/cilib-old/V7R1_B75D099SP/trunk/'
            },
            {
                label: '标题模板(普通)',
                type: 'copy',
                content: '【SR88B75D086SP维护分支】【模块 问题级别】'
            },
            {
                label: '标题模板(BRAS)',
                type: 'copy',
                content: '【SR88B75D086SP维护分支】【BRAS领域】【模块 问题级别】'
            },
            {
                label: '审批人',
                type: 'copy',
                content: 'yanglan@h3c.com zhangzongsheng.07463@h3c.com'
            }
        ]
    }
};