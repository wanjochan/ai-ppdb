import os
import sqlite3
from datetime import datetime
from contextlib import contextmanager
from typing import Optional, List, Dict, Any

class DBManager:
    def __init__(self, db_path="./ai/var/ppdb.sqlite"):
        self.db_path = db_path
        os.makedirs(os.path.dirname(db_path), exist_ok=True)
        self._init_db()

    def _init_db(self):
        """初始化数据库"""
        with open("ai/schema.sql", "r") as f:
            schema = f.read()
        
        with self.get_connection() as conn:
            conn.executescript(schema)
            conn.commit()

    @contextmanager
    def get_connection(self):
        """获取数据库连接"""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
        finally:
            conn.close()

    def create_session(self, role: str) -> int:
        """创建新会话"""
        now = datetime.now()
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                INSERT INTO sessions (current_role, started_at, last_active_at, status)
                VALUES (?, ?, ?, 'ACTIVE')
                """,
                (role, now, now)
            )
            conn.commit()
            return cursor.lastrowid

    def update_session(self, session_id: int, role: str = None, task_id: str = None,
                      ide_pid: int = None, status: str = None) -> None:
        """更新会话状态"""
        updates = []
        params = []
        if role is not None:
            updates.append("current_role = ?")
            params.append(role)
        if task_id is not None:
            updates.append("current_task_id = ?")
            params.append(task_id)
        if ide_pid is not None:
            updates.append("ide_pid = ?")
            params.append(ide_pid)
        if status is not None:
            updates.append("status = ?")
            params.append(status)
        
        updates.append("last_active_at = ?")
        params.append(datetime.now())
        params.append(session_id)

        with self.get_connection() as conn:
            conn.execute(
                f"""
                UPDATE sessions
                SET {', '.join(updates)}
                WHERE id = ?
                """,
                params
            )
            conn.commit()

    def add_session_history(self, session_id: int, role: str, task_id: str, action: str) -> None:
        """添加会话历史记录"""
        with self.get_connection() as conn:
            conn.execute(
                """
                INSERT INTO session_history (session_id, role, task_id, action, timestamp)
                VALUES (?, ?, ?, ?, ?)
                """,
                (session_id, role, task_id, action, datetime.now())
            )
            conn.commit()

    def get_active_session(self) -> Optional[Dict[str, Any]]:
        """获取活动会话"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                SELECT * FROM sessions
                WHERE status = 'ACTIVE'
                ORDER BY last_active_at DESC
                LIMIT 1
                """
            )
            row = cursor.fetchone()
            return dict(row) if row else None

    def create_task(self, from_role: str, to_role: str, subject: str, content: str,
                   task_id: str, reply_to: str = None) -> None:
        """创建新任务"""
        now = datetime.now()
        with self.get_connection() as conn:
            conn.execute(
                """
                INSERT INTO tasks (id, from_role, to_role, subject, content, status,
                                 created_at, reply_to)
                VALUES (?, ?, ?, ?, ?, 'UNREAD', ?, ?)
                """,
                (task_id, from_role, to_role, subject, content, now, reply_to)
            )
            conn.execute(
                """
                INSERT INTO task_status_history (task_id, status, timestamp, note)
                VALUES (?, 'UNREAD', ?, 'Task created')
                """,
                (task_id, now)
            )
            conn.commit()

    def update_task_status(self, task_id: str, status: str, note: str = None) -> None:
        """更新任务状态"""
        now = datetime.now()
        with self.get_connection() as conn:
            updates = ["status = ?", "last_active_at = ?"]
            params = [status, now]
            
            if status == 'UNREAD':
                updates.append("read_at = NULL")
            elif status != 'RECALLED' and self.get_task_status(task_id) == 'UNREAD':
                updates.append("read_at = ?")
                params.append(now)
            elif status == 'RECALLED':
                updates.append("recalled_at = ?")
                params.append(now)
            
            params.append(task_id)
            
            conn.execute(
                f"""
                UPDATE tasks
                SET {', '.join(updates)}
                WHERE id = ?
                """,
                params
            )
            
            conn.execute(
                """
                INSERT INTO task_status_history (task_id, status, timestamp, note)
                VALUES (?, ?, ?, ?)
                """,
                (task_id, status, now, note or f"Status changed to {status}")
            )
            conn.commit()

    def get_task_status(self, task_id: str) -> Optional[str]:
        """获取任务状态"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                "SELECT status FROM tasks WHERE id = ?",
                (task_id,)
            )
            row = cursor.fetchone()
            return row['status'] if row else None

    def get_tasks_by_role(self, role: str, status: str = None,
                         include_sent: bool = False) -> List[Dict[str, Any]]:
        """获取角色的任务"""
        query = """
            SELECT * FROM tasks
            WHERE (to_role = ?
        """
        params = [role]
        
        if include_sent:
            query += " OR from_role = ?"
            params.append(role)
            
        query += ")"
        
        if status:
            query += " AND status = ?"
            params.append(status)
            
        query += " ORDER BY created_at DESC"
        
        with self.get_connection() as conn:
            cursor = conn.execute(query, params)
            return [dict(row) for row in cursor.fetchall()]

    def get_task_history(self, task_id: str) -> List[Dict[str, Any]]:
        """获取任务状态历史"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                SELECT * FROM task_status_history
                WHERE task_id = ?
                ORDER BY timestamp
                """,
                (task_id,)
            )
            return [dict(row) for row in cursor.fetchall()]

    def delete_completed_tasks(self) -> int:
        """删除所有已完成的任务
        
        Returns:
            int: 删除的任务数量
        """
        with self.get_connection() as conn:
            # 先删除历史记录
            conn.execute(
                """
                DELETE FROM task_status_history
                WHERE task_id IN (
                    SELECT id FROM tasks
                    WHERE status = 'COMPLETED'
                )
                """
            )
            
            # 删除会话历史
            conn.execute(
                """
                DELETE FROM session_history
                WHERE task_id IN (
                    SELECT id FROM tasks
                    WHERE status = 'completed'
                )
                """
            )
            
            # 最后删除任务
            cursor = conn.execute(
                """
                DELETE FROM tasks
                WHERE status = 'completed'
                """
            )
            
            deleted_count = cursor.rowcount
            conn.commit()
            return deleted_count