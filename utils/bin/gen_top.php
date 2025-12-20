<?php
	if (!isset($_SERVER["argc"]))
	{
		echo ("Invalid usage");
		exit();
	}

	chdir(dirname($_SERVER["argv"][0]));

	require_once "../lib/db_open.inc.php";
	require_once "../lib/str_process.inc.php";

	$text_file_path = "../../var/bbs_top.txt";
	$menu_file_path = "../../var/bbs_top_menu.conf";

	$buffer_text =
		"                     \033[1;34m-----\033[37m=====\033[41;37m 本站十大热门话题 \033[40m=====\033[34m-----\033[m\n\n";

	$buffer_screen = <<<SCREEN
#---------------------------------------------------------------------
%S_TOP10
                     \033[1;34m-----\033[37m=====\033[41;37m 本站十大热门话题 \033[40m=====\033[34m-----\033[m

SCREEN;

	$buffer_menu = <<<MENU
#---------------------------------------------------------------------
%menu M_TOP10
title       0, 0, "十大热门话题"
screen      2, 0, S_TOP10

MENU;

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

		$line_section = sprintf (
			" \033[1;37m第 \033[31m%2d \033[37m名 版块 : \033[33m%s [%s]%s \033[37m【 \033[32m%s \033[37m】\033[35m%s%s ",
			$i,
			$row["s_title"],
			$row["sname"],
			str_repeat(" ", 20 - str_length($row["s_title"]) - strlen($row["sname"])),
			(new DateTimeImmutable($row["sub_dt"]))->format("M d H:i:s"),
			str_repeat(" ", 16 - strlen($row["username"])),
			$row["username"]
			);

		$line_article = sprintf (
			" \033[1;37m       标题 : \033[44;37m%s%s  \033[0;40;37m",
			$title_f,
			str_repeat(" ", 60 - str_length($title_f))
			);

		$buffer_text .= $line_section . "\n  " . $line_article . "\n";

		$buffer_screen .= $line_section . "\n\n";

		$row_article = $i * 2 + 2;

		$buffer_menu .= <<<MENU
@LOCATE_ARTICLE   {$row_article}, 3, 1, 0,   "{$row['sname']}|{$row['AID']}",    "{$line_article}"

MENU;

		$i++;
	}
	mysqli_free_result($rs);

	mysqli_close($db_conn);

	$buffer_screen .= "%\n";
	$buffer_menu .= ("%\n" . $buffer_screen);

	file_put_contents($text_file_path, $buffer_text);
	file_put_contents($menu_file_path, $buffer_menu);
