<?php
	include "../conf/db_conn.conf.php";

	function db_open($sql_charset = "utf8")
	{
		global $DB_hostname, $DB_username, $DB_password, $DB_database, $DB_session_timezone;

		mysqli_report(MYSQLI_REPORT_OFF);

		$mysqli = @new mysqli($DB_hostname, $DB_username, $DB_password, $DB_database);

		if ($mysqli->connect_errno)
		{
			echo ("Mysqli connection error: " . $mysqli->connect_error);
			return NULL;
		}

		$mysqli->set_charset($sql_charset);
		if ($mysqli->errno) {
			echo ("Mysqli error: " . $mysqli->error . "\n");
		}

		if (isset($DB_session_timezone))
		{
			$mysqli->query("SET time_zone = '" . $DB_session_timezone . "'");
			if ($mysqli->errno) {
				echo ("Mysqli error: " . $mysqli->error . "\n");
			}
		}

		return $mysqli;
	}

	if (!isset($db_conn))
	{
		$db_conn = db_open();

		if ($db_conn == NULL)
		{
			exit();
		}
	}
