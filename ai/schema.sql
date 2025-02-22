-- 邮件表
CREATE TABLE IF NOT EXISTS mails (
    id TEXT PRIMARY KEY,  -- mail_id (timestamp_hash)
    from_role TEXT NOT NULL,
    to_role TEXT NOT NULL,
    subject TEXT NOT NULL,
    content TEXT NOT NULL,
    status TEXT NOT NULL,  -- UNREAD/PROCESSING/REPLIED/COMPLETED/ARCHIVED/RECALLED
    created_at TIMESTAMP NOT NULL,
    read_at TIMESTAMP,
    recalled_at TIMESTAMP,
    reply_to TEXT,  -- 引用的邮件ID
    FOREIGN KEY (reply_to) REFERENCES mails(id)
);

-- 邮件状态历史表
CREATE TABLE IF NOT EXISTS mail_status_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mail_id TEXT NOT NULL,
    status TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    note TEXT,
    FOREIGN KEY (mail_id) REFERENCES mails(id)
);

-- 会话表
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    current_role TEXT NOT NULL,  -- 当前角色
    current_mail_id TEXT,  -- 当前处理的邮件ID
    ide_pid INTEGER,  -- IDE进程ID
    started_at TIMESTAMP NOT NULL,
    last_active_at TIMESTAMP NOT NULL,
    status TEXT NOT NULL,  -- ACTIVE/PAUSED/COMPLETED
    FOREIGN KEY (current_mail_id) REFERENCES mails(id)
);

-- 会话历史表
CREATE TABLE IF NOT EXISTS session_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    role TEXT NOT NULL,
    mail_id TEXT NOT NULL,
    action TEXT NOT NULL,  -- READ/PROCESS/REPLY/COMPLETE
    timestamp TIMESTAMP NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id),
    FOREIGN KEY (mail_id) REFERENCES mails(id)
); 