<?php
	if (!isset($_SERVER["argc"]))
	{
		echo ("Invalid usage");
		exit();
	}

	chdir(dirname($_SERVER["argv"][0]));

	require_once "../lib/db_open.inc.php";
	require_once "../lib/str_process.inc.php";

	$cache_path = "../../var/bbs_top.txt";

	$buffer =
		"               \033[1;34m-----\033[37m=====\033[41;37m 本站十大热门话题 \033[40m=====\033[34m-----\033[m\r\n\r\n";

	$sql = "SELECT AID, bbs.title AS title, sname,
			section_config.title AS s_title, username, sub_dt
			FROM bbs INNER JOIN section_config ON bbs.SID = section_config.SID
			WHERE section_config.recommend AND TID = 0 AND visible AND view_count >= 10
			AND (sub_dt >= SUBDATE(NOW(), INTERVAL 7 DAY))
			ORDER BY excerption DESC, (view_count + reply_count) DESC, transship
			LIMIT 10";

	$rs = mysqli_query($db_conn, $sql);
	if ($rs == false)
	{
		echo("Query data error: " . mysqli_error($db_conn));
		exit();
	}

	$i = 1;
	while ($row = mysqli_fetch_array($rs))
	{
		$title_f = split_line($row["title"], "", 60, 1, "");

		$buffer .= sprintf (
			" \033[1;37m第 \033[31m%2d \033[37m名 版块 : \033[33m%s [%s]%s \033[37m【 \033[32m%s \033[37m】\033[35m%s%s \n" .
			" \033[37m         标题 : \033[44;37m%s%s  \033[0;40;37m \n",
			$i++,
			$row["s_title"],
			$row["sname"],
			str_repeat(" ", 20 - str_length($row["s_title"]) - strlen($row["sname"])),
			(new DateTimeImmutable($row["sub_dt"]))->format("M d H:i:s"),
			str_repeat(" ", 16 - strlen($row["username"])),
			$row["username"],
			$title_f,
			str_repeat(" ", 60 - str_length($title_f))
			);
	}
	mysqli_free_result($rs);

	mysqli_close($db_conn);

	file_put_contents($cache_path, $buffer);
