#-*- coding: utf-8 -*-
# # 用【邮件模式】来实现最简单的任务管理系统
"""
获取邮箱中的邮件
python ai/mailer.py --mode check --role Engineer
python ai/mailer.py --mode check --role ProjectManager
python ai/mailer.py --mode check --role User

发送邮件
python ai/mailer.py --mode send --from-role User --to-role ProjectManager --subject "Test Mail" --content "This is a test mail"

更新邮件状态
python ai/mailer.py --mode update-status --role ProjectManager --mail-id 20250222_175714_315453e3 --status recalled

撤回邮件
python ai/mailer.py --mode recall --from-role ProjectManager --mail-id 20250222_175714_315453e3

"""

import sys
import os
import time
import json
from datetime import datetime
import hashlib
import argparse
from enum import Enum, auto
from typing import Optional, List, Dict, Any

# 修改为绝对导入
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from ai.db import DBManager

class MailStatus(Enum):
    """邮件状态枚举"""
    UNREAD = "unread"
    PROCESSING = "processing"
    REPLIED = "replied" 
    COMPLETED = "completed"
    ARCHIVED = "archived"
    RECALLED = "recalled"  # 新增：已撤回状态

class MailboxManager:
    def __init__(self):
        self.db = DBManager()
        self.roles = ["User", "ProjectManager", "Engineer"]

    def _generate_mail_id(self, content: str) -> str:
        """生成邮件ID"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        return f"{timestamp}_{content_hash}"

    def send_mail(self, from_role: str, to_role: str, subject: str,
                 content: str, reply_to: str = None) -> str:
        """发送邮件"""
        if from_role not in self.roles or to_role not in self.roles:
            raise ValueError(f"Invalid role: {from_role} or {to_role}")

        mail_id = self._generate_mail_id(content)
        self.db.create_mail(from_role, to_role, subject, content, mail_id, reply_to)
        return mail_id

    def recall_mail(self, from_role: str, mail_id: str) -> None:
        """撤回邮件（仅限未读邮件）"""
        # 获取邮件
        mails = self.db.get_mails_by_role(from_role, include_sent=True)
        mail = next((m for m in mails if m['id'] == mail_id), None)
        
        if not mail:
            raise FileNotFoundError(f"Mail not found: {mail_id}")
        
        # 只能撤回发给别人的邮件
        if mail['from_role'] != from_role:
            raise ValueError("Can only recall mails you sent")
        
        # 如果邮件已读，不能撤回
        if mail.get('read_at'):
            raise ValueError("Cannot recall mail that has been read")
        
        # 更新状态为已撤回
        self.db.update_mail_status(mail_id, MailStatus.RECALLED.value,
                                 "Mail recalled by sender")

    def get_mails_by_status(self, role: str, status: Optional[MailStatus] = None,
                           include_sent: bool = False) -> List[Dict[str, Any]]:
        """获取指定状态的邮件"""
        if role not in self.roles:
            raise ValueError(f"Invalid role: {role}")

        return self.db.get_mails_by_role(
            role,
            status=status.value if status else None,
            include_sent=include_sent
        )

    def update_mail_status(self, role: str, mail_id: str,
                          new_status: MailStatus, note: str = None) -> None:
        """更新邮件状态"""
        if not isinstance(new_status, MailStatus):
            raise ValueError(f"Invalid status: {new_status}")

        # 检查邮件是否存在且属于该角色
        mails = self.db.get_mails_by_role(role, include_sent=True)
        mail = next((m for m in mails if m['id'] == mail_id), None)
        
        if not mail:
            raise FileNotFoundError(f"Mail not found or no permission: {mail_id}")
        
        self.db.update_mail_status(mail_id, new_status.value, note)

def check_mailbox(role: str, status: Optional[str] = None,
                 include_sent: bool = False) -> None:
    """检查邮箱中的指定状态邮件"""
    manager = MailboxManager()
    status_enum = MailStatus(status) if status else None
    mails = manager.get_mails_by_status(role, status_enum, include_sent)
    
    if not mails:
        status_msg = f" with status {status}" if status else ""
        sent_msg = " (including sent)" if include_sent else ""
        print(json.dumps({
            "status": "no_mail", 
            "message": f"No mails{status_msg}{sent_msg} for {role}"
        }, ensure_ascii=False))
        return

    print(json.dumps({
        "status": "has_mail",
        "mails": [{
            "mail_id": mail['id'],
            "from": mail['from_role'],
            "to": mail['to_role'],
            "subject": mail['subject'],
            "content": mail['content'],
            "status": mail['status'],
            "created_at": mail['created_at'],
            "read_at": mail.get('read_at'),
            "recalled_at": mail.get('recalled_at'),
            "reply_to": mail.get('reply_to')
        } for mail in mails]
    }, ensure_ascii=False))

def main():
    parser = argparse.ArgumentParser(description='Mailbox Manager')
    parser.add_argument('--mode', choices=['check', 'send', 'update-status', 'recall'],
                      required=True,
                      help='Operation mode')
    parser.add_argument('--role', help='Role to check mailbox for')
    parser.add_argument('--status', choices=[s.value for s in MailStatus],
                      help='Mail status to filter or update to')
    parser.add_argument('--from-role', help='Sender role when sending mail')
    parser.add_argument('--to-role', help='Recipient role when sending mail')
    parser.add_argument('--subject', help='Mail subject when sending')
    parser.add_argument('--content', help='Mail content when sending')
    parser.add_argument('--mail-id', help='Mail ID for status update or recall')
    parser.add_argument('--reply-to', help='Original mail ID when sending a reply')
    parser.add_argument('--note', help='Note for status update')
    parser.add_argument('--include-sent', action='store_true',
                      help='Include sent mails when checking')
    
    args = parser.parse_args()
    manager = MailboxManager()

    if args.mode == 'check':
        if not args.role:
            print(json.dumps({"status": "error", "message": "Role is required for check mode"},
                           ensure_ascii=False))
            return
        check_mailbox(args.role, args.status, args.include_sent)
    
    elif args.mode == 'send':
        if not all([args.from_role, args.to_role, args.subject, args.content]):
            print(json.dumps({"status": "error",
                            "message": "from_role, to_role, subject and content are required"},
                           ensure_ascii=False))
            return
        mail_id = manager.send_mail(args.from_role, args.to_role, args.subject,
                                  args.content, args.reply_to)
        print(json.dumps({"status": "sent", "mail_id": mail_id}, ensure_ascii=False))
    
    elif args.mode == 'update-status':
        if not all([args.role, args.mail_id, args.status]):
            print(json.dumps({"status": "error",
                            "message": "role, mail_id and status are required"},
                           ensure_ascii=False))
            return
        manager.update_mail_status(args.role, args.mail_id,
                                 MailStatus(args.status), args.note)
        print(json.dumps({"status": "updated", "mail_id": args.mail_id,
                         "new_status": args.status}, ensure_ascii=False))

    elif args.mode == 'recall':
        if not all([args.from_role, args.mail_id]):
            print(json.dumps({"status": "error",
                            "message": "from_role and mail_id are required"},
                           ensure_ascii=False))
            return
        try:
            manager.recall_mail(args.from_role, args.mail_id)
            print(json.dumps({"status": "recalled", "mail_id": args.mail_id},
                           ensure_ascii=False))
        except (FileNotFoundError, ValueError) as e:
            print(json.dumps({"status": "error", "message": str(e)},
                           ensure_ascii=False))

if __name__ == '__main__':
    main()