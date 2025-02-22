#-*- coding: utf-8 -*-
"""
任务管理系统核心组件

功能：
- 任务的创建、更新、查询等业务操作
- 数据库操作
- 状态管理
- 命令行接口

使用方法：
python ai/tasker.py --mode check --role Dev
python ai/tasker.py --mode check --role PM
python ai/tasker.py --mode check --role User
python ai/tasker.py --mode send --from-role User --to-role PM --subject "Test Task" --content "This is a test task"
python ai/tasker.py --mode update-status --role PM --task-id <task_id> --status processing
python ai/tasker.py --mode recall --from-role PM --task-id <task_id>
"""

import sys
import os
import json
import argparse
import hashlib
from datetime import datetime
from typing import Optional, Dict, Any, List
from enum import Enum

# 添加项目根目录到 Python 路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from ai.taskdb import DBManager

class TaskStatus(str, Enum):
    """任务状态枚举"""
    UNREAD = "unread"
    PROCESSING = "processing"
    REPLIED = "replied" 
    COMPLETED = "completed"
    ARCHIVED = "archived"
    RECALLED = "recalled"

class TaskManager:
    def __init__(self):
        self.db = DBManager()
        self.roles = ["User", "PM", "Dev"]

    def _generate_task_id(self, content: str) -> str:
        """生成任务ID"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        return f"{timestamp}_{content_hash}"

    def send_task(self, from_role: str, to_role: str, subject: str,
                 content: str, reply_to: str = None) -> str:
        """发送任务"""
        if from_role not in self.roles or to_role not in self.roles:
            raise ValueError(f"Invalid role: {from_role} or {to_role}")

        task_id = self._generate_task_id(content)
        self.db.create_task(from_role, to_role, subject, content, task_id, reply_to)
        return task_id

    def recall_task(self, from_role: str, task_id: str) -> None:
        """撤回任务（仅限未读任务）"""
        tasks = self.db.get_tasks_by_role(from_role, include_sent=True)
        task = next((m for m in tasks if m['id'] == task_id), None)
        
        if not task:
            raise ValueError(f"Task not found: {task_id}")
        
        if task['from_role'] != from_role:
            raise ValueError("Can only recall tasks you sent")
        
        if task.get('read_at'):
            raise ValueError("Cannot recall task that has been read")
        
        self.db.update_task_status(task_id, TaskStatus.RECALLED.value,
                                 "Task recalled by sender")

    def get_tasks_by_status(self, role: str, status: Optional[TaskStatus] = None,
                           include_sent: bool = False) -> List[Dict[str, Any]]:
        """获取指定状态的任务"""
        if role not in self.roles:
            raise ValueError(f"Invalid role: {role}")

        return self.db.get_tasks_by_role(
            role,
            status=status.value if status else None,
            include_sent=include_sent
        )

    def update_task_status(self, role: str, task_id: str,
                          new_status: TaskStatus, note: str = None) -> None:
        """更新任务状态"""
        tasks = self.db.get_tasks_by_role(role, include_sent=True)
        task = next((m for m in tasks if m['id'] == task_id), None)
        
        if not task:
            raise ValueError(f"Task not found or no permission: {task_id}")
        
        self.db.update_task_status(task_id, new_status.value, note)

def format_task_output(task: Dict[str, Any]) -> Dict[str, Any]:
    """格式化任务输出"""
    return {
        "task_id": task['id'],
        "from": task['from_role'],
        "to": task['to_role'],
        "subject": task['subject'],
        "content": task['content'],
        "status": task['status'],
        "created_at": task['created_at'],
        "read_at": task.get('read_at'),
        "recalled_at": task.get('recalled_at'),
        "reply_to": task.get('reply_to'),
        "last_active_at": task.get('last_active_at')
    }

def check_tasks(task_manager: TaskManager, role: str, status: Optional[str] = None,
                include_sent: bool = False) -> None:
    """检查任务"""
    status_enum = TaskStatus(status) if status else None
    try:
        tasks = task_manager.get_tasks_by_status(role, status_enum, include_sent)
        
        if not tasks:
            status_msg = f" with status {status}" if status else ""
            sent_msg = " (including sent)" if include_sent else ""
            print(json.dumps({
                "status": "no_task", 
                "message": f"No tasks{status_msg}{sent_msg} for {role}"
            }, ensure_ascii=False))
            return

        print(json.dumps({
            "status": "has_task",
            "tasks": [format_task_output(task) for task in tasks]
        }, ensure_ascii=False))

    except ValueError as e:
        print(json.dumps({
            "status": "error",
            "message": str(e)
        }, ensure_ascii=False))

def main():
    parser = argparse.ArgumentParser(description='Task Manager')
    parser.add_argument('--mode', choices=['check', 'send', 'update-status', 'recall', 'check-changes'],
                      required=True,
                      help='Operation mode')
    parser.add_argument('--role', help='Role to check tasks for')
    parser.add_argument('--status', choices=[s.value for s in TaskStatus],
                      help='Task status to filter or update to')
    parser.add_argument('--from-role', help='Sender role when sending task')
    parser.add_argument('--to-role', help='Recipient role when sending task')
    parser.add_argument('--subject', help='Task subject when sending')
    parser.add_argument('--content', help='Task content when sending')
    parser.add_argument('--task-id', help='Task ID for status update or recall')
    parser.add_argument('--reply-to', help='Original task ID when sending a reply')
    parser.add_argument('--note', help='Note for status update')
    parser.add_argument('--include-sent', action='store_true',
                      help='Include sent tasks when checking')
    parser.add_argument('--json-output', action='store_true',
                      help='Always output in JSON format')
    parser.add_argument('--last-id', type=int, default=0,
                      help='Last change ID when checking changes')
    
    args = parser.parse_args()
    task_manager = TaskManager()

    try:
        if args.mode == 'check':
            if not args.role:
                raise ValueError("Role is required for check mode")
            check_tasks(task_manager, args.role, args.status, args.include_sent)
        
        elif args.mode == 'check-changes':
            # 检查变更
            with task_manager.db.get_connection() as conn:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT id FROM changes 
                    WHERE id > ? 
                    ORDER BY id DESC 
                    LIMIT 1
                """, (args.last_id,))
                row = cursor.fetchone()
                
                if row:
                    print(json.dumps({
                        "status": "has_changes",
                        "last_id": row[0]
                    }, ensure_ascii=False))
                else:
                    print(json.dumps({
                        "status": "no_changes",
                        "last_id": args.last_id
                    }, ensure_ascii=False))
        
        elif args.mode == 'send':
            if not all([args.from_role, args.to_role, args.subject, args.content]):
                raise ValueError("from_role, to_role, subject and content are required")
            task_id = task_manager.send_task(args.from_role, args.to_role,
                                           args.subject, args.content, args.reply_to)
            print(json.dumps({"status": "success", "task_id": task_id}, ensure_ascii=False))
        
        elif args.mode == 'update-status':
            if not all([args.role, args.task_id, args.status]):
                raise ValueError("role, task_id and status are required")
            task_manager.update_task_status(args.role, args.task_id,
                                          TaskStatus(args.status), args.note)
            print(json.dumps({"status": "success"}, ensure_ascii=False))

        elif args.mode == 'recall':
            if not all([args.from_role, args.task_id]):
                raise ValueError("from_role and task_id are required")
            task_manager.recall_task(args.from_role, args.task_id)
            print(json.dumps({"status": "success"}, ensure_ascii=False))

    except Exception as e:
        print(json.dumps({
            "status": "error",
            "message": str(e)
        }, ensure_ascii=False))
        sys.exit(1)

if __name__ == '__main__':
    main()
