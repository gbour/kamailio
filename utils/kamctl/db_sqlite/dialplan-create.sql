INSERT INTO version (table_name, table_version) values ('dialplan','1');
CREATE TABLE dialplan (
    id INTEGER PRIMARY KEY NOT NULL,
    dpid INTEGER NOT NULL,
    pr INTEGER NOT NULL,
    match_op INTEGER NOT NULL,
    match_exp VARCHAR(64) NOT NULL,
    match_len INTEGER NOT NULL,
    subst_exp VARCHAR(64) NOT NULL,
    repl_exp VARCHAR(32) NOT NULL,
    attrs VARCHAR(32) NOT NULL
);

