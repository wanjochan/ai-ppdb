-- 任务表
CREATE TABLE IF NOT EXISTS tasks (
    id TEXT PRIMARY KEY,  -- task_id (timestamp_hash)
    from_role TEXT NOT NULL,
    to_role TEXT NOT NULL,
    subject TEXT NOT NULL,
    content TEXT NOT NULL,
    status TEXT NOT NULL,  -- UNREAD/PROCESSING/REPLIED/COMPLETED/ARCHIVED/RECALLED
    created_at TIMESTAMP NOT NULL,
    read_at TIMESTAMP,
    recalled_at TIMESTAMP,
    reply_to TEXT,  -- 引用的任务ID
    last_active_at TIMESTAMP,  -- 最后活动时间
    FOREIGN KEY (reply_to) REFERENCES tasks(id)
);

-- 变更跟踪表
CREATE TABLE IF NOT EXISTS changes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    operation TEXT NOT NULL,  -- INSERT/UPDATE/DELETE
    record_id TEXT NOT NULL,
    changed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 任务变更触发器
CREATE TRIGGER IF NOT EXISTS task_insert_trigger AFTER INSERT ON tasks
BEGIN
    INSERT INTO changes (table_name, operation, record_id)
    VALUES ('tasks', 'INSERT', NEW.id);
END;

CREATE TRIGGER IF NOT EXISTS task_update_trigger AFTER UPDATE ON tasks
BEGIN
    INSERT INTO changes (table_name, operation, record_id)
    VALUES ('tasks', 'UPDATE', NEW.id);
END;

CREATE TRIGGER IF NOT EXISTS task_delete_trigger AFTER DELETE ON tasks
BEGIN
    INSERT INTO changes (table_name, operation, record_id)
    VALUES ('tasks', 'DELETE', OLD.id);
END;

-- 任务状态历史表
CREATE TABLE IF NOT EXISTS task_status_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id TEXT NOT NULL,
    status TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    note TEXT,
    FOREIGN KEY (task_id) REFERENCES tasks(id)
);

-- 会话表
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    current_role TEXT NOT NULL,  -- 当前角色
    current_task_id TEXT,  -- 当前处理的任务ID
    ide_pid INTEGER,  -- IDE进程ID
    started_at TIMESTAMP NOT NULL,
    last_active_at TIMESTAMP NOT NULL,
    status TEXT NOT NULL,  -- ACTIVE/PAUSED/COMPLETED
    FOREIGN KEY (current_task_id) REFERENCES tasks(id)
);

-- 会话历史表
CREATE TABLE IF NOT EXISTS session_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    role TEXT NOT NULL,
    task_id TEXT NOT NULL,
    action TEXT NOT NULL,  -- READ/PROCESS/REPLY/COMPLETE
    timestamp TIMESTAMP NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id),
    FOREIGN KEY (task_id) REFERENCES tasks(id)
); 