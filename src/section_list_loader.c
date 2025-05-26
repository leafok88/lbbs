/***************************************************************************
					section_list_loader.c  -  description
							 -------------------
	Copyright            : (C) 2004-2025 by Leaflet
	Email                : leaflet@leafok.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "section_list_loader.h"
#include "log.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int load_section_config_from_db(MYSQL *db)
{
	MYSQL_RES *rs, *rs2;
	MYSQL_ROW row, row2;
	char sql[SQL_BUFFER_LEN];
	int32_t sid;
	char master_name[BBS_username_max_len + 1];
	SECTION_LIST *p_section;
	int ret;

	snprintf(sql, sizeof(sql),
			 "SELECT section_config.SID, sname, title, section_config.CID, read_user_level, write_user_level, "
			 "section_config.enable * section_class.enable AS enable "
			 "FROM section_config INNER JOIN section_class ON section_config.CID = sectioN_class.CID "
			 "ORDER BY section_config.SID");

	if (mysql_query(db, sql) != 0)
	{
		log_error("Query section_list error: %s\n", mysql_error(db));
		return -1;
	}
	if ((rs = mysql_store_result(db)) == NULL)
	{
		log_error("Get section_list data failed\n");
		return -1;
	}
	while ((row = mysql_fetch_row(rs)))
	{
		sid = atoi(row[0]);

		// Query section master
		snprintf(sql, sizeof(sql),
				 "SELECT username FROM section_master "
				 "INNER JOIN user_list ON section_master.UID = user_list.UID "
				 "WHERE SID = %d AND section_master.enable AND (NOW() BETWEEN begin_dt AND end_dt) "
				 "ORDER BY major DESC LIMIT 1",
				 sid);

		if (mysql_query(db, sql) != 0)
		{
			log_error("Query section_master error: %s\n", mysql_error(db));
			return -2;
		}
		if ((rs2 = mysql_store_result(db)) == NULL)
		{
			log_error("Get section_master data failed\n");
			return -2;
		}
		if ((row2 = mysql_fetch_row(rs2)))
		{
			strncpy(master_name, row2[0], sizeof(master_name) - 1);
			master_name[sizeof(master_name) - 1] = '\0';
		}
		else
		{
			master_name[0] = '\0';
		}
		mysql_free_result(rs2);

		p_section = section_list_find_by_sid(sid);

		if (p_section == NULL)
		{
			p_section = section_list_create(sid, row[1], row[2], "");
			if (p_section == NULL)
			{
				log_error("load_section_config_from_db() error: load new section sid = %d sname = %s\n", sid, row[1]);
				break;
			}

			// acquire rw lock
			ret = section_list_rw_lock(p_section);
			if (ret < 0)
			{
				break;
			}
		}
		else
		{
			// acquire rw lock
			ret = section_list_rw_lock(p_section);
			if (ret < 0)
			{
				break;
			}

			strncpy(p_section->sname, row[1], sizeof(p_section->sname) - 1);
			p_section->sname[sizeof(p_section->sname) - 1] = '\0';
			strncpy(p_section->stitle, row[1], sizeof(p_section->stitle) - 1);
			p_section->stitle[sizeof(p_section->stitle) - 1] = '\0';
			strncpy(p_section->master_name, master_name, sizeof(p_section->master_name) - 1);
			p_section->master_name[sizeof(p_section->master_name) - 1] = '\0';
		}

		p_section->class_id = atoi(row[3]);
		p_section->read_user_level = atoi(row[4]);
		p_section->write_user_level = atoi(row[5]);
		p_section->enable = (int8_t)atoi(row[6]);

		// release rw lock
		ret = section_list_rw_unlock(p_section);
		if (ret < 0)
		{
			break;
		}
	}
	mysql_free_result(rs);

	return 0;
}
