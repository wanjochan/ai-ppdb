#-*- coding: utf-8 -*-
# # 用【邮件模式】来实现最简单的任务管理系统

import sys
import os
import time
import json
from datetime import datetime
import hashlib
import argparse
from enum import Enum, auto

class MailStatus(Enum):
    """邮件状态枚举"""
    UNREAD = "unread"
    PROCESSING = "processing"
    REPLIED = "replied" 
    COMPLETED = "completed"
    ARCHIVED = "archived"
    RECALLED = "recalled"  # 新增：已撤回状态

class MailboxManager:
    def __init__(self, base_dir="./ai/var/mailboxes"):
        self.base_dir = base_dir
        self.roles = ["User", "ProjectManager", "Engineer"]
        self._ensure_directories()

    def _ensure_directories(self):
        """确保所需目录存在"""
        for role in self.roles:
            role_dir = os.path.join(self.base_dir, role)
            for subdir in ['inbox', 'outbox', 'archive']:
                os.makedirs(os.path.join(role_dir, subdir), exist_ok=True)

    def _generate_mail_id(self, content):
        """生成邮件ID"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        return f"{timestamp}_{content_hash}.json"

    def _get_mail_path(self, role, mail_id, folder=None):
        """获取邮件文件路径"""
        if folder:
            return os.path.join(self.base_dir, role, folder, mail_id)
        
        # 在所有文件夹中查找
        for f in ['inbox', 'outbox', 'archive']:
            path = os.path.join(self.base_dir, role, f, mail_id)
            if os.path.exists(path):
                return path
        return None

    def send_mail(self, from_role, to_role, subject, content, reply_to=None):
        """发送邮件"""
        if from_role not in self.roles or to_role not in self.roles:
            raise ValueError(f"Invalid role: {from_role} or {to_role}")

        mail = {
            "from": from_role,
            "to": to_role,
            "subject": subject,
            "content": content,
            "status": MailStatus.UNREAD.value,
            "created_at": datetime.now().isoformat(),
            "read_at": None,  # 新增：首次阅读时间
            "recalled_at": None,  # 新增：撤回时间
            "status_history": [{
                "status": MailStatus.UNREAD.value,
                "timestamp": datetime.now().isoformat(),
                "note": "Mail created"
            }],
            "reply_to": reply_to,
            "replies": []
        }

        mail_id = self._generate_mail_id(content)
        
        # 在发件人的outbox中保存副本
        outbox_path = os.path.join(self.base_dir, from_role, "outbox", mail_id)
        with open(outbox_path, 'w', encoding='utf-8') as f:
            json.dump(mail, f, ensure_ascii=False, indent=2)

        # 在收件人的inbox中保存邮件
        inbox_path = os.path.join(self.base_dir, to_role, "inbox", mail_id)
        with open(inbox_path, 'w', encoding='utf-8') as f:
            json.dump(mail, f, ensure_ascii=False, indent=2)

        if reply_to:
            self._update_original_mail_replies(from_role, reply_to, mail_id)
        
        return mail_id

    def _update_original_mail_replies(self, role, original_mail_id, reply_mail_id):
        """更新原邮件的回复列表"""
        # 在inbox、outbox和archive中查找原邮件
        for folder in ['inbox', 'outbox', 'archive']:
            mail_path = os.path.join(self.base_dir, role, folder, original_mail_id)
            if os.path.exists(mail_path):
                with open(mail_path, 'r', encoding='utf-8') as f:
                    mail = json.load(f)
                mail['replies'].append(reply_mail_id)
                with open(mail_path, 'w', encoding='utf-8') as f:
                    json.dump(mail, f, ensure_ascii=False, indent=2)
                break

    def recall_mail(self, from_role, mail_id):
        """撤回邮件（仅限未读邮件）"""
        # 检查发件箱中的邮件
        outbox_path = self._get_mail_path(from_role, mail_id, 'outbox')
        if not outbox_path:
            raise FileNotFoundError(f"Mail not found in outbox: {mail_id}")

        with open(outbox_path, 'r', encoding='utf-8') as f:
            mail = json.load(f)

        # 只能撤回发给别人的邮件
        if mail['from'] != from_role:
            raise ValueError("Can only recall mails you sent")

        # 获取收件人收件箱中的邮件
        to_role = mail['to']
        inbox_path = self._get_mail_path(to_role, mail_id, 'inbox')
        
        # 如果邮件已经不在收件箱，说明已经被读过或者归档了
        if not inbox_path or mail.get('read_at'):
            raise ValueError("Cannot recall mail that has been read or archived")

        # 更新状态为已撤回
        now = datetime.now().isoformat()
        mail['status'] = MailStatus.RECALLED.value
        mail['recalled_at'] = now
        mail['status_history'].append({
            "status": MailStatus.RECALLED.value,
            "timestamp": now,
            "note": "Mail recalled by sender"
        })

        # 更新发件人的发件箱
        with open(outbox_path, 'w', encoding='utf-8') as f:
            json.dump(mail, f, ensure_ascii=False, indent=2)

        # 更新收件人的收件箱
        with open(inbox_path, 'w', encoding='utf-8') as f:
            json.dump(mail, f, ensure_ascii=False, indent=2)

    def get_mails_by_status(self, role, status=None, include_outbox=False):
        """获取指定状态的邮件"""
        if role not in self.roles:
            raise ValueError(f"Invalid role: {role}")

        if status and not isinstance(status, MailStatus):
            raise ValueError(f"Invalid status: {status}")

        result_mails = []
        
        # 获取收件箱邮件
        inbox_path = os.path.join(self.base_dir, role, "inbox")
        for filename in os.listdir(inbox_path):
            if not filename.endswith('.json'):
                continue

            mail_path = os.path.join(inbox_path, filename)
            with open(mail_path, 'r', encoding='utf-8') as f:
                mail = json.load(f)
                if not status or mail['status'] == status.value:
                    mail['mail_id'] = filename
                    mail['location'] = 'inbox'
                    result_mails.append(mail)

        # 如果需要，也获取发件箱邮件
        if include_outbox:
            outbox_path = os.path.join(self.base_dir, role, "outbox")
            for filename in os.listdir(outbox_path):
                if not filename.endswith('.json'):
                    continue

                mail_path = os.path.join(outbox_path, filename)
                with open(mail_path, 'r', encoding='utf-8') as f:
                    mail = json.load(f)
                    if not status or mail['status'] == status.value:
                        mail['mail_id'] = filename
                        mail['location'] = 'outbox'
                        result_mails.append(mail)

        return result_mails

    def update_mail_status(self, role, mail_id, new_status, note=None):
        """更新邮件状态"""
        if not isinstance(new_status, MailStatus):
            raise ValueError(f"Invalid status: {new_status}")

        # 查找邮件
        mail_path = self._get_mail_path(role, mail_id)
        if not mail_path:
            raise FileNotFoundError(f"Mail not found: {mail_id}")

        # 读取邮件
        with open(mail_path, 'r', encoding='utf-8') as f:
            mail = json.load(f)

        now = datetime.now().isoformat()
        
        # 如果是首次阅读，记录阅读时间
        if mail['status'] == MailStatus.UNREAD.value and new_status != MailStatus.RECALLED.value:
            mail['read_at'] = now
            
            # 同步更新发件人的发件箱中的邮件状态
            sender_outbox_path = self._get_mail_path(mail['from'], mail_id, 'outbox')
            if sender_outbox_path:
                with open(sender_outbox_path, 'r', encoding='utf-8') as f:
                    sender_copy = json.load(f)
                sender_copy['read_at'] = now
                with open(sender_outbox_path, 'w', encoding='utf-8') as f:
                    json.dump(sender_copy, f, ensure_ascii=False, indent=2)

        # 更新状态
        mail['status'] = new_status.value
        mail['status_history'].append({
            "status": new_status.value,
            "timestamp": now,
            "note": note or f"Status changed to {new_status.value}"
        })

        # 如果状态是ARCHIVED，移动到archive文件夹
        if new_status == MailStatus.ARCHIVED:
            archive_path = os.path.join(os.path.dirname(os.path.dirname(mail_path)), 'archive', mail_id)
            with open(archive_path, 'w', encoding='utf-8') as f:
                json.dump(mail, f, ensure_ascii=False, indent=2)
            if 'inbox' in mail_path:
                os.remove(mail_path)
        else:
            with open(mail_path, 'w', encoding='utf-8') as f:
                json.dump(mail, f, ensure_ascii=False, indent=2)

def check_mailbox(role, status=None, include_outbox=False):
    """检查邮箱中的指定状态邮件"""
    manager = MailboxManager()
    status_enum = MailStatus(status) if status else None
    mails = manager.get_mails_by_status(role, status_enum, include_outbox)
    
    if not mails:
        status_msg = f" with status {status}" if status else ""
        location_msg = " (including outbox)" if include_outbox else ""
        print(json.dumps({
            "status": "no_mail", 
            "message": f"No mails{status_msg}{location_msg} for {role}"
        }, ensure_ascii=False))
        return

    print(json.dumps({
        "status": "has_mail",
        "mails": [{
            "mail_id": mail['mail_id'],
            "from": mail['from'],
            "to": mail['to'],
            "subject": mail['subject'],
            "content": mail['content'],
            "status": mail['status'],
            "location": mail['location'],
            "created_at": mail['created_at'],
            "read_at": mail.get('read_at'),
            "recalled_at": mail.get('recalled_at'),
            "reply_to": mail.get('reply_to'),
            "replies": mail.get('replies', [])
        } for mail in mails]
    }, ensure_ascii=False))

def main():
    parser = argparse.ArgumentParser(description='Mailbox Manager')
    parser.add_argument('--mode', choices=['check', 'send', 'update-status', 'recall'], required=True,
                      help='Operation mode: check (check mails), send (send new mail), update-status (update mail status), recall (recall unread mail)')
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
    parser.add_argument('--include-outbox', action='store_true',
                      help='Include outbox mails when checking')
    
    args = parser.parse_args()
    manager = MailboxManager()

    if args.mode == 'check':
        if not args.role:
            print(json.dumps({"status": "error", "message": "Role is required for check mode"}, ensure_ascii=False))
            return
        check_mailbox(args.role, args.status, args.include_outbox)
    
    elif args.mode == 'send':
        if not all([args.from_role, args.to_role, args.subject, args.content]):
            print(json.dumps({"status": "error", "message": "from_role, to_role, subject and content are required for sending mail"}, ensure_ascii=False))
            return
        mail_id = manager.send_mail(args.from_role, args.to_role, args.subject, args.content, args.reply_to)
        print(json.dumps({"status": "sent", "mail_id": mail_id}, ensure_ascii=False))
    
    elif args.mode == 'update-status':
        if not all([args.role, args.mail_id, args.status]):
            print(json.dumps({"status": "error", "message": "role, mail_id and status are required for status update"}, ensure_ascii=False))
            return
        manager.update_mail_status(args.role, args.mail_id, MailStatus(args.status), args.note)
        print(json.dumps({"status": "updated", "mail_id": args.mail_id, "new_status": args.status}, ensure_ascii=False))

    elif args.mode == 'recall':
        if not all([args.from_role, args.mail_id]):
            print(json.dumps({"status": "error", "message": "from_role and mail_id are required for recalling mail"}, ensure_ascii=False))
            return
        try:
            manager.recall_mail(args.from_role, args.mail_id)
            print(json.dumps({"status": "recalled", "mail_id": args.mail_id}, ensure_ascii=False))
        except (FileNotFoundError, ValueError) as e:
            print(json.dumps({"status": "error", "message": str(e)}, ensure_ascii=False))

if __name__ == '__main__':
    main()