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

    def update_session(self, session_id: int, role: str = None, mail_id: str = None,
                      ide_pid: int = None, status: str = None) -> None:
        """更新会话状态"""
        updates = []
        params = []
        if role is not None:
            updates.append("current_role = ?")
            params.append(role)
        if mail_id is not None:
            updates.append("current_mail_id = ?")
            params.append(mail_id)
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

    def add_session_history(self, session_id: int, role: str, mail_id: str, action: str) -> None:
        """添加会话历史记录"""
        with self.get_connection() as conn:
            conn.execute(
                """
                INSERT INTO session_history (session_id, role, mail_id, action, timestamp)
                VALUES (?, ?, ?, ?, ?)
                """,
                (session_id, role, mail_id, action, datetime.now())
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

    def create_mail(self, from_role: str, to_role: str, subject: str, content: str,
                   mail_id: str, reply_to: str = None) -> None:
        """创建新邮件"""
        now = datetime.now()
        with self.get_connection() as conn:
            conn.execute(
                """
                INSERT INTO mails (id, from_role, to_role, subject, content, status,
                                 created_at, reply_to)
                VALUES (?, ?, ?, ?, ?, 'UNREAD', ?, ?)
                """,
                (mail_id, from_role, to_role, subject, content, now, reply_to)
            )
            conn.execute(
                """
                INSERT INTO mail_status_history (mail_id, status, timestamp, note)
                VALUES (?, 'UNREAD', ?, 'Mail created')
                """,
                (mail_id, now)
            )
            conn.commit()

    def update_mail_status(self, mail_id: str, status: str, note: str = None) -> None:
        """更新邮件状态"""
        now = datetime.now()
        with self.get_connection() as conn:
            updates = ["status = ?", "last_active_at = ?"]
            params = [status, now]
            
            if status == 'UNREAD':
                updates.append("read_at = NULL")
            elif status != 'RECALLED' and self.get_mail_status(mail_id) == 'UNREAD':
                updates.append("read_at = ?")
                params.append(now)
            elif status == 'RECALLED':
                updates.append("recalled_at = ?")
                params.append(now)
            
            params.append(mail_id)
            
            conn.execute(
                f"""
                UPDATE mails
                SET {', '.join(updates)}
                WHERE id = ?
                """,
                params
            )
            
            conn.execute(
                """
                INSERT INTO mail_status_history (mail_id, status, timestamp, note)
                VALUES (?, ?, ?, ?)
                """,
                (mail_id, status, now, note or f"Status changed to {status}")
            )
            conn.commit()

    def get_mail_status(self, mail_id: str) -> Optional[str]:
        """获取邮件状态"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                "SELECT status FROM mails WHERE id = ?",
                (mail_id,)
            )
            row = cursor.fetchone()
            return row['status'] if row else None

    def get_mails_by_role(self, role: str, status: str = None,
                         include_sent: bool = False) -> List[Dict[str, Any]]:
        """获取角色的邮件"""
        query = """
            SELECT * FROM mails
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

    def get_mail_history(self, mail_id: str) -> List[Dict[str, Any]]:
        """获取邮件状态历史"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                SELECT * FROM mail_status_history
                WHERE mail_id = ?
                ORDER BY timestamp
                """,
                (mail_id,)
            )
            return [dict(row) for row in cursor.fetchall()] 