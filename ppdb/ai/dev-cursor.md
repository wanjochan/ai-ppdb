# AI开发模式说明

## 1. 角色定义

- **PM-Cursor**: 负责任务规划、监控和协调。注意：PM角色不能直接编辑任务日志和状态文件，这些文件由工程师实例负责更新
- **工程师-Cursor**: 负责具体任务执行、代码实现，以及维护任务日志和状态文件

## 2. 工作流程

### 2.1 任务定义
PM-Cursor 创建标准格式的任务文件，包含:
```json
{
    "task_id": "001",
    "type": "code_implementation",
    "description": "任务描述",
    "priority": "high/medium/low",
    "project_context": {
        "root_dir": "项目根目录",
        "source_dir": "源码目录",
        "test_dir": "测试目录",
        "scripts_dir": "脚本目录"
    },
    "reporting": {
        "required": true,
        "interval": "per_step",
        "files": {
            "log": "日志文件路径",
            "status": "状态文件路径"
        },
        "rules": [
            "状态更新规则1",
            "状态更新规则2"
        ]
    },
    "steps": [
        {
            "step": 1,
            "action": "动作描述",
            "tool": "使用的工具",
            "params": {
                "参数1": "值1"
            },
            "log_required": {
                "before": "执行前日志模板",
                "during": "执行中日志模板",
                "after": "执行后日志模板",
                "details": "详细信息模板"
            }
        }
    ],
    "status": "pending",
    "error_handling": {
        "on_missing_logs": "pause_and_notify",
        "on_status_timeout": "request_update"
    }
}
```

### 2.2 任务执行
工程师-Cursor 执行任务时必须:
1. 及时更新状态文件
2. 按要求记录详细日志
3. 遵循项目规范
4. 保持代码质量

### 2.3 监控反馈
PM-Cursor 负责:
1. 监控任务执行状态
2. 检查日志更新情况
3. 在异常情况下介入
4. 确保任务按计划完成

## 3. 文件结构

```
ppdb/ai/
├── dev/                # 开发相关文件
│   ├── tasks/         # 任务定义文件
│   ├── logs/          # 执行日志
│   └── status/        # 状态文件
└── dev-cursor.md      # 本说明文档
```

## 4. 注意事项

1. 所有任务必须有明确的ID和完整的上下文信息
2. 日志必须包含时间戳和足够的细节
3. 状态更新间隔不得超过5分钟
4. 出现异常情况必须立即报告
5. 代码变更必须有清晰的原因和影响说明
6. PM角色只能读取而不能修改任务日志和状态文件，这些文件的更新权限仅属于工程师实例

## 5. 最佳实践

1. 任务拆分要合理，每个任务都要可执行和可验证
2. 日志要详细但不冗余
3. 状态更新要及时且准确
4. 代码修改要谨慎，保持向后兼容
5. 测试覆盖要完整

## 6. 错误处理

1. 日志缺失处理
2. 状态超时处理
3. 任务异常处理
4. 代码冲突处理

## 7. 质量保证

1. 代码审查要求
2. 测试覆盖要求
3. 性能要求
4. 文档要求 

## 8. 实施细节

### 8.1 任务文件命名规范
```
ppdb/ai/dev/tasks/task{编号}.txt
示例: task001.txt, task002.txt
```

### 8.2 日志格式规范
```
[时间戳] [步骤编号] [动作] 具体内容
示例: [2024-01-11T10:30:00Z] [1] [搜索代码] 开始搜索内存池相关代码
```

### 8.3 状态文件格式
```json
{
    "task_id": "001",
    "status": "in_progress/completed/failed",
    "current_step": 1,
    "start_time": "2024-01-11T10:00:00Z",
    "last_update": "2024-01-11T10:05:00Z",
    "steps_completed": [],
    "current_action": "动作描述",
    "progress": "进展描述",
    "findings": {
        "key": "value"
    }
}
```

### 8.4 异常处理流程

1. **日志缺失**:
   - PM-Cursor 每5分钟检查一次日志更新
   - 如无更新，标记任务状态为 "waiting_for_update"
   - 通知工程师-Cursor 补充日志

2. **任务超时**:
   - 定义每个步骤的预期完成时间
   - 超过预期时间50%，发出警告
   - 超过预期时间100%，暂停任务并评估

3. **代码冲突**:
   - 发现冲突时立即暂停当前任务
   - 记录冲突细节到日志
   - PM-Cursor 介入协调解决

### 8.5 通信协议

1. **任务分派**:
   ```json
   {
       "type": "task_assignment",
       "task_id": "001",
       "task_file": "ppdb/ai/dev/tasks/task001.txt",
       "priority": "high",
       "deadline": "2024-01-12T18:00:00Z"
   }
   ```

2. **状态报告**:
   ```json
   {
       "type": "status_report",
       "task_id": "001",
       "timestamp": "2024-01-11T10:30:00Z",
       "status": "in_progress",
       "progress": "80%",
       "current_step": 2,
       "issues": []
   }
   ```

3. **异常报告**:
   ```json
   {
       "type": "error_report",
       "task_id": "001",
       "timestamp": "2024-01-11T10:35:00Z",
       "error_type": "compilation_error",
       "details": "编译错误详情",
       "stack_trace": "错误堆栈"
   }
   ```

### 8.6 代码审查清单

1. **基本检查**:
   - 代码格式是否符合规范
   - 是否包含必要的注释
   - 是否添加了测试用例
   - 是否更新了相关文档

2. **功能检查**:
   - 功能是否完整实现
   - 边界条件是否处理
   - 错误处理是否完善
   - 性能是否符合要求

3. **安全检查**:
   - 内存管理是否安全
   - 并发处理是否正确
   - 输入验证是否充分
   - 错误处理是否安全

### 8.7 性能指标

1. **响应时间**:
   - 任务接收确认: < 1分钟
   - 状态更新间隔: < 5分钟
   - 异常响应时间: < 2分钟

2. **质量指标**:
   - 代码覆盖率: > 90%
   - 内存泄漏: 0
   - 并发安全: 100%
   - 向后兼容: 100% 