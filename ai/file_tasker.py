# -*- coding: utf-8 -*-
"""
基于文件系统的任务管理器

功能:
- 使用CSV文件存储任务信息
- 通过文件系统目录结构管理任务状态
- 提供命令行接口进行任务管理

命令行用法:
python ai/file_tasker.py check --role Dev
python ai/file_tasker.py check --role PM
python ai/file_tasker.py send --from-role User --to-role PM --subject "Test Task" --content "This is a test task"
"""

import os
import csv
import sys
import json
import shutil
import hashlib
import argparse
from datetime import datetime
from typing import Optional, Dict, Any, List
from enum import Enum
from pathlib import Path

# 基础目录
BASE_DIR = Path(__file__).parent / "var" / "tasks"

class TaskStatus(str, Enum):
    """任务状态枚举"""
    NEW = "new"
    PROCESSING = "processing"
    COMPLETED = "completed" 
    ARCHIVED = "archived"

class StatusCode:
    """状态码常量"""
    OK = "0"  # 成功
    NO_TASK = "1"  # 无任务
    ERROR = "2"  # 错误

class TaskManager:
    def __init__(self):
        self.roles = ["User", "PM", "Dev", "DS"]
        self._ensure_dirs()

    def _ensure_dirs(self):
        """确保所需目录存在"""
        for status in TaskStatus:
            (BASE_DIR / status.value).mkdir(parents=True, exist_ok=True)

    def _generate_task_id(self, content: str) -> str:
        """生成任务ID"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        return f"{timestamp}_{content_hash}"

    def _get_task_path(self, task_id: str, status: TaskStatus) -> Path:
        """获取任务文件路径"""
        return BASE_DIR / status.value / f"{task_id}.csv"

    def send_task(self, from_role: str, to_role: str, subject: str,
                 content: str, reply_to: str = None) -> str:
        """发送任务"""
        if from_role not in self.roles or to_role not in self.roles:
            raise ValueError(f"Invalid role: {from_role} or {to_role}")

        task_id = self._generate_task_id(content)
        task_data = {
            "task_id": task_id,
            "from_role": from_role,
            "to_role": to_role,
            "subject": subject,
            "content": content,
            "created_at": datetime.now().isoformat(),
            "status": TaskStatus.NEW.value,
            "reply_to": reply_to
        }

        task_path = self._get_task_path(task_id, TaskStatus.NEW)
        with open(task_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=task_data.keys())
            writer.writeheader()
            writer.writerow(task_data)

        return task_id

    def get_tasks(self, role: str, status: Optional[TaskStatus] = None) -> List[Dict[str, Any]]:
        """获取任务列表"""
        if role not in self.roles:
            raise ValueError(f"Invalid role: {role}")

        tasks = []
        statuses = [status] if status else list(TaskStatus)
        
        for s in statuses:
            status_dir = BASE_DIR / s.value
            if not status_dir.exists():
                continue

            for task_file in status_dir.glob("*.csv"):
                with open(task_file, "r", newline="") as f:
                    reader = csv.DictReader(f)
                    task = next(reader)
                    if task["to_role"] == role:
                        tasks.append(task)

        return tasks

    def update_task_status(self, role: str, task_id: str,
                          new_status: TaskStatus) -> None:
        """更新任务状态"""
        # 查找任务文件
        task_file = None
        for status in TaskStatus:
            path = self._get_task_path(task_id, status)
            if path.exists():
                task_file = path
                break

        if not task_file:
            raise ValueError(f"Task not found: {task_id}")

        # 读取任务数据
        with open(task_file, "r", newline="") as f:
            reader = csv.DictReader(f)
            task = next(reader)

        if task["to_role"] != role:
            raise ValueError("No permission to update this task")

        # 更新状态
        task["status"] = new_status.value
        new_path = self._get_task_path(task_id, new_status)

        # 写入新文件
        with open(new_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=task.keys())
            writer.writeheader()
            writer.writerow(task)

        # 删除旧文件
        task_file.unlink()

def format_task_output(task: Dict[str, Any]) -> str:
    """格式化任务输出（简化版）"""
    return f"{task['task_id']}|{task['from_role']}|{task['to_role']}|{task['status']}|{task['subject']}"

def format_task_detail(task: Dict[str, Any]) -> str:
    """格式化任务详细信息"""
    return (f"ID: {task['task_id']}\n"
            f"From: {task['from_role']}\n"
            f"To: {task['to_role']}\n"
            f"Subject: {task['subject']}\n"
            f"Status: {task['status']}\n"
            f"Content: {task['content']}\n"
            f"Created: {task['created_at']}")

def output_result(status_code: str, data: Any = None, message: str = "") -> None:
    """输出结果"""
    if status_code == StatusCode.OK:
        print(f"{status_code}:{data if data is not None else ''}")
    elif status_code == StatusCode.ERROR:
        print(f"{status_code}:{message}")
    else:
        print(f"{status_code}:")

def check_tasks(task_manager: TaskManager, role: str, status: Optional[str] = None,
                show_detail: bool = False) -> None:
    """检查任务"""
    status_enum = TaskStatus(status) if status else None
    try:
        tasks = task_manager.get_tasks(role, status_enum)
        
        if not tasks:
            output_result(StatusCode.NO_TASK)
            return

        if show_detail:
            task_list = [format_task_detail(task) for task in tasks]
        else:
            task_list = [format_task_output(task) for task in tasks]
            
        output_result(StatusCode.OK, "\n".join(task_list))

    except ValueError as e:
        output_result(StatusCode.ERROR, message=str(e))

def main():
    parser = argparse.ArgumentParser(description='File-based Task Manager')
    
    # 添加子命令
    subparsers = parser.add_subparsers(dest='mode', help='Operation mode')
    
    # 检查任务命令
    check_parser = subparsers.add_parser('check', help='Check tasks')
    check_parser.add_argument('--role', required=True, help='Role to check tasks for')
    check_parser.add_argument('--status', choices=[s.value for s in TaskStatus],
                           help='Task status to filter')
    check_parser.add_argument('--detail', action='store_true',
                           help='Show detailed task information')
    
    # 发送任务命令
    send_parser = subparsers.add_parser('send', help='Send task')
    send_parser.add_argument('--from-role', required=True, help='Sender role')
    send_parser.add_argument('--to-role', required=True, help='Recipient role')
    send_parser.add_argument('--subject', required=True, help='Task subject')
    send_parser.add_argument('--content', required=True, help='Task content')
    send_parser.add_argument('--reply-to', help='Original task ID')
    
    # 更新状态命令
    update_parser = subparsers.add_parser('update-status', help='Update task status')
    update_parser.add_argument('--role', required=True, help='Role updating the task')
    update_parser.add_argument('--task-id', required=True, help='Task ID')
    update_parser.add_argument('--status', required=True,
                            choices=[s.value for s in TaskStatus],
                            help='New status')
    
    args = parser.parse_args()
    task_manager = TaskManager()

    try:
        if args.mode == 'check':
            check_tasks(task_manager, args.role, args.status, args.detail)
        
        elif args.mode == 'send':
            task_id = task_manager.send_task(args.from_role, args.to_role,
                                           args.subject, args.content, args.reply_to)
            output_result(StatusCode.OK, task_id)
        
        elif args.mode == 'update-status':
            task_manager.update_task_status(args.role, args.task_id,
                                          TaskStatus(args.status))
            output_result(StatusCode.OK)
        
        else:
            parser.print_help()
            sys.exit(1)

    except Exception as e:
        output_result(StatusCode.ERROR, message=str(e))
        sys.exit(1)

if __name__ == '__main__':
    main() 