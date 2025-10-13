#ifdef CONSTANTS_H
#define CONSTANTS_H

// 文章相关
#define BBS_article_title_max_len           160
#define BBS_article_limit_per_section       50000
#define BBS_article_limit_per_page          20
#define BBS_ontop_article_limit_per_section 10
#define ARTICLE_PER_BLOCK 1000

#define TITLE_INPUT_MAX_LEN                 72
#define ARTICLE_CONTENT_MAX_LEN             1024 * 1024 * 4 // 4MB
#define ARTICLE_QUOTE_MAX_LINES             20
#define ARTICLE_QUOTE_LINE_MAX_LEN          76
#define MODIFY_DT_MAX_LEN                   50

// SQL相关
#define SQL_ROLLBACK                        "ROLLBACK"
#define SQL_AUTOCOMMIT_ON                   "SET autocommit=1"

#endif
