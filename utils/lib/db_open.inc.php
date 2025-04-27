<?
	include "../conf/db_conn.conf.php";

	$db_conn_id=mysql_connect($DB_hostname, $DB_username, $DB_password)
		or die("Could not connect database");
	mysql_select_db($DB_database,$db_conn_id) or die("Invalid database");
	mysql_query("SET CHARACTER SET gb2312", $db_conn_id);
	mysql_query("SET NAMES 'gb2312'", $db_conn_id);

	return $db_conn_id;
?>
