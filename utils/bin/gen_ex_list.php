<?php
	function gen_ex_dir(mysqli $db_conn, int $sid, int $fid = 0, string $dir = "", string $name = "") : string | bool
	{
		$output = "";
		$output_cur_dir = "";

		// Generate menu of current dir
		$output_cur_dir .= <<<MENU
#---------------------------------------------------------------------
%menu M_EX_{$sid}_{$fid}
title       0, 0, "[{$name}]"
screen      2, 0, S_EX_DIR
page        4, 1, 20

MENU;

		$sql = "SELECT FID, dir, name, dt FROM ex_dir WHERE SID = $sid AND dir REGEXP '^" .
				mysqli_real_escape_string($db_conn, $dir) .
				"[^/]+/$' AND enable ORDER BY dir";
		$rs = mysqli_query($db_conn, $sql);
		if ($rs == false)
		{
			echo ("Query dir_list error: " . mysqli_error($db_conn));
			return false;
		}

		$display_row = 4;
		while ($row = mysqli_fetch_array($rs))
		{
			$output_sub_dir = gen_ex_dir($db_conn, $sid, $row["FID"], $row["dir"], $row["name"]);
			if ($output_sub_dir == false)
			{
				echo ("gen_ex_dir({$sid}, {$row['dir']}) error\n");
				return false;
			}
			$output .= $output_sub_dir;

			$dt = date("YÄêmÔÂdÈÕ", strtotime($row["dt"]));

			$output_cur_dir .= <<<MENU
			!M_EX_{$sid}_{$row['FID']}   {$display_row}, 4, 1, 0, "{$row['dir']}", " [Ä¿Â¼]  {$row['name']} [{$dt}]"

			MENU;

			$display_row = 0;
		}
		mysqli_free_result($rs);
	
		$sql = "SELECT bbs.AID, title, sub_dt FROM bbs
				LEFT JOIN ex_file ON bbs.AID = ex_file.AID
				LEFT JOIN ex_dir ON ex_file.FID = ex_dir.FID
				WHERE bbs.SID = $sid AND TID = 0 AND visible AND gen_ex
				AND IF(dir IS NULL, '', dir) = '" .
				mysqli_real_escape_string($db_conn, $dir) .
				"' ORDER BY bbs.AID";
		$rs = mysqli_query($db_conn, $sql);
		if ($rs == false)
		{
			echo ("Query article error: " . mysqli_error($db_conn));
			return false;
		}

		while ($row = mysqli_fetch_array($rs))
		{
			$aid_f = str_repeat(" ", 7 - strlen("{$row['AID']}")) . "{$row['AID']}";
			$title_f = split_line($row["title"], "", 50, 1, "");
			$title_f = addslashes($title_f) . str_repeat(" ", 50 - str_length($title_f));
			$sub_dt = date("YÄêmÔÂdÈÕ", strtotime($row["sub_dt"]));

			$output_cur_dir .= <<<MENU
			@VIEW_EX_ARTICLE   {$display_row}, 4, 1, 0, "{$row['AID']}", "{$aid_f}  {$title_f} [{$sub_dt}]"

			MENU;

			$display_row = 0;
		}
		mysqli_free_result($rs);

		$output_cur_dir .= <<<MENU
%

MENU;

		$output = $output_cur_dir . $output;

		return $output;
	}

	if (!isset($_SERVER["argc"]))
	{
		echo ("Invalid usage");
		exit();
	}

	chdir(dirname($_SERVER["argv"][0]));

	require_once "../lib/db_open.inc.php";
	require_once "../lib/str_process.inc.php";

	// Generate menu screen
	$menu_screen = <<<MENU
#---------------------------------------------------------------------
%S_EX_DIR
  ·µ»Ø[[1;32m¡û[0;37m] ½øÈë[[1;32m¡ú[0;37m] Ñ¡Ôñ[[1;32m¡ü PgUp[0;37m,[1;32m¡ý PgDn[0;37m]
[44;37m    [1;37m±à  ºÅ  ÎÄÕÂ±êÌâ                                           ÈÕ  ÆÚ           [m




















%

MENU;

	$sql = "SELECT section_config.SID, section_config.title AS s_title
			FROM section_config
			INNER JOIN section_class ON	section_config.CID = section_class.CID
			WHERE section_config.enable AND section_class.enable AND ex_menu_update
			ORDER BY section_class.sort_order, section_config.sort_order";

	$rs = mysqli_query($db_conn, $sql);
	if ($rs == false)
	{
		echo ("Query section error: " . mysqli_error($db_conn));
		exit();
	}

	while ($row = mysqli_fetch_array($rs))
	{
		$buffer = gen_ex_dir($db_conn, $row["SID"], 0, "", $row["s_title"]);

		if ($buffer !== false)
		{
			$menu_output_path = "../../var/gen_ex/" . $row["SID"];
			file_put_contents($menu_output_path, $menu_screen . $buffer);

			$sql = "UPDATE section_config SET ex_menu_tm = NOW(), ex_menu_update = 0 WHERE SID = " . $row["SID"];
			$ret = mysqli_query($db_conn, $sql);
			if ($ret == false)
			{
				echo ("Update tm error: " . mysqli_error($db_conn));
				exit();
			}
		}
		else
		{
			echo ("gen_ex_dir({$row["SID"]}, /) error\n");
		}
	}
	mysqli_free_result($rs);

	mysqli_close($db_conn);
