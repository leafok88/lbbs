<?php
	if (!isset($_SERVER["argc"]))
	{
		echo ("Invalid usage");
		exit();
	}

	chdir(dirname($_SERVER["argv"][0]));

	require_once "../lib/db_open.inc.php";
	require_once "../lib/str_process.inc.php";
	require_once "../lib/section_list.inc.php";

	$menu_input_path = "../../conf/menu.conf";
	$menu_output_path = "../../var/menu_merged.conf";

	$buffer = file_get_contents($menu_input_path);
	if ($buffer == false)
	{
		echo "Unable to load menu config file " . $menu_input_path . "\n";
		exit();
	}

	// Load section list
	$section_hierachy = array();

	$ret = load_section_list($section_hierachy,
		function (array $section, array $filter_param) : bool
		{
			return true;
		},
		function (array $section, array $filter_param) : mixed
		{
			$db_conn = $filter_param["db"];
			$result = array(
				"article_count" => 0,
				"section_master" => "",
			);

			// Query article count
			$sql = "SELECT COUNT(*) AS article_count FROM bbs WHERE SID = " .
					$section["SID"] . " AND visible";
			
			$rs = mysqli_query($db_conn, $sql);
			if ($rs == false)
			{
				echo mysqli_error($db_conn);
				return $result;
			}

			if ($row = mysqli_fetch_array($rs))
			{
				$result["article_count"] = $row["article_count"];
			}
			mysqli_free_result($rs);

			// Query section master
			$sql = "SELECT user_list.UID, user_list.username, section_master.major FROM section_master
					INNER JOIN user_list ON section_master.UID = user_list.UID WHERE SID = " .
					$section["SID"] . " AND section_master.enable AND (NOW() BETWEEN begin_dt AND end_dt)
					ORDER BY major DESC LIMIT 1";

			$rs = mysqli_query($db_conn, $sql);
			if ($rs == false)
			{
				echo mysqli_error($db_conn);
				return $result;
			}

			if ($row = mysqli_fetch_array($rs))
			{
				$result["section_master"] = $row["username"];
			}
			mysqli_free_result($rs);

			return $result;
		},
		$db_conn,
		array(
			"db" => $db_conn
		)
	);

	if ($ret == false)
	{
		echo "Query section list error: " . mysqli_error($db_conn);
		exit();
	}

	// Generate menu of section class
	$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M_EGROUP
title       0, 0, "[栏目列表]"
screen      2, 0, S_EGROUP

MENU;

	foreach ($section_hierachy as $c_index => $section_class)
	{
		$display_row = ($c_index == 0 ? 4 : 0);

		$section_count = count($section_class["sections"]);

		$title_f = str_repeat(" ", 5 - intval(log10($section_count))) . $section_count . " ＋  " .
			$section_class['name'] . str_repeat(" ", 32 - strlen($section_class['name'])) .
			"[" . addslashes($section_class['title']) . "]";

		$buffer .= <<<MENU
		!M__{$section_class['name']}   {$display_row}, 4, 1, 0,   "{$section_class['name']}",    "{$title_f}"

		MENU;
	}

	$buffer .= <<<MENU
%

MENU;

	foreach ($section_hierachy as $c_index => $section_class)
	{
		// Generate menu of sections in section_class[$c_index]
		$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M__{$section_class["name"]}
title       0, 0, "[{$section_class["title"]}]"
screen      2, 0, S_BOARD

MENU;

		$class_title_f = "[" . addslashes($section_class['title']) . "]" . str_repeat(" ", 10 - str_length($section_class['title']));

		foreach ($section_class["sections"] as $s_index => $section)
		{
			$display_row = ($s_index == 0 ? 4 : 0);

			$article_count = $section['udf_values']['article_count'];

			$title_f = str_repeat(" ", 5 - intval(log10($article_count))) . $article_count . " ＋  " .
				$section['name'] . str_repeat(" ", 20 - strlen($section['name'])) .
				$class_title_f . addslashes($section['title']) . str_repeat(" ", 22 - str_length($section['title'])) .
				$section['udf_values']['section_master'];

			$buffer .= <<<MENU
			@LIST_SECTION   {$display_row}, 4, 1, {$section['read_user_level']},   "{$section['name']}",    "{$title_f}"

			MENU;
		}

		$buffer .= <<<MENU
%

MENU;

	}

	// Generate menu of all sections
	$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M_BOARD
title       0, 0, "[版块列表]"
screen      2, 0, S_BOARD
page        4, 1, 20

MENU;

	$display_row = 4;

	foreach ($section_hierachy as $c_index => $section_class)
	{
		$class_title_f = "[" . addslashes($section_class['title']) . "]" . str_repeat(" ", 10 - str_length($section_class['title']));

		foreach ($section_class["sections"] as $s_index => $section)
		{
			$article_count = $section['udf_values']['article_count'];

			$title_f = str_repeat(" ", 5 - intval(log10($article_count))) . $article_count . " ＋  " .
				$section['name'] . str_repeat(" ", 20 - strlen($section['name'])) .
				$class_title_f . addslashes($section['title']) . str_repeat(" ", 22 - str_length($section['title'])) .
				$section['udf_values']['section_master'];

			$buffer .= <<<MENU
			@LIST_SECTION   {$display_row}, 4, 1, {$section['read_user_level']},   "{$section['name']}",    "{$title_f}"

			MENU;

			$display_row = 0;
		}
	}

	$buffer .= <<<MENU
%

MENU;

	unset($section_hierachy);

	mysqli_close($db_conn);

	file_put_contents($menu_output_path, $buffer);
