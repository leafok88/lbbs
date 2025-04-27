<?
	if (!isset($_SERVER["argc"]))
		$_SERVER["argc"] = 0;

	if (isset($_SERVER["argv"][0]))
		chdir(substr($_SERVER["argv"][0],0,strrpos($_SERVER["argv"][0],"/")));

	$cache_path = "../../data/bbs_top.txt";

	$buffer =
		"               \033[1;34m-----\033[37m=====\033[41;37m 本站十大热门话题 \033[40m=====\033[34m-----\033[m\r\n\r\n";

	$db_conn = include "./db_open.inc.php";
	
	$rs = mysql_query(
		" select bbs.title, sname, username, sub_dt".
		" from bbs inner join section_config on".
		" bbs.SID=section_config.SID where section_config.recommend".
		" and bbs.TID=0 and bbs.visible and bbs.view_count>=10".
		" and (bbs.sub_dt >= subdate(now(),interval '7' day))".
		" order by bbs.excerption desc,bbs.view_count+bbs.reply_count desc,".
		"bbs.transship limit 10",$db_conn)
		or die ("Query data error");

	$i = 1;
	while ($row=mysql_fetch_array($rs))
	{
		$buffer .= sprintf (
			" \033[1;37m第 \033[31m%2d \033[37m名 版块 : \033[33m%s%s \033[37m【\033[32m%s \033[37m】    \033[35m%s%s \r\n".
			" \033[37m     标题 : \033[44;37m%s%s \033[0;40;37m\r\n",
			$i++, $row["sname"], str_repeat(" ", 20 - strlen($row["sname"])),
			strftime("%b %d %H:%M:%S", strtotime($row["sub_dt"])),
			str_repeat(" ", 16 - strlen($row["username"])),
			$row["username"], substr($row["title"],0,60), 
			str_repeat(" ", 60 - strlen($row["title"]) >=0 ? 60 - strlen($row["title"]) : 0)
			);
	}
	
	mysql_free_result($rs);

	mysql_close($db_conn);

	if (($fp=fopen($cache_path,"w")))
	{
		fwrite($fp,$buffer);
		fclose($fp);
	}
	
	return 0;
?>
