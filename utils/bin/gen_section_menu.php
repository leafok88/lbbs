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
				($section['udf_values']['section_master'] == "" ? "诚征版主中" : $section['udf_values']['section_master']);

			$buffer .= <<<MENU
			@LIST_SECTION   {$display_row}, 4, 1, {$section['read_user_level']},   "{$section['name']}",    "{$title_f}"

			MENU;
		}

		$buffer .= <<<MENU
%

MENU;

	}

	// Load excerptional section list
	$ex_section_hierachy = array();

	$ret = load_section_list($ex_section_hierachy,
		function (array $section, array $filter_param) : bool
		{
			$db_conn = $filter_param["db"];

			// Query excerptional article count
			$sql = "SELECT AID AS ex_article_count FROM bbs WHERE SID = " .
					$section["SID"] . " AND visible AND gen_ex LIMIT 1";
			
			$rs = mysqli_query($db_conn, $sql);
			if ($rs == false)
			{
				echo mysqli_error($db_conn);
				return false;
			}

			$have_ex_article = (mysqli_num_rows($rs) > 0);
			mysqli_free_result($rs);

			return $have_ex_article;
		},
		function (array $section, array $filter_param) : mixed
		{
			$db_conn = $filter_param["db"];
			$result = array(
				"ex_menu_tm" => 0,
				"ex_article_count" => 0,
			);

			// Query ex_menu_tm
			$sql = "SELECT ex_menu_tm FROM section_config WHERE SID = " . $section["SID"];
			$rs = mysqli_query($db_conn, $sql);
			if ($rs == false)
			{
				echo mysqli_error($db_conn);
				return $result;
			}

			if ($row = mysqli_fetch_array($rs))
			{
				$result["ex_menu_tm"] = $row["ex_menu_tm"];
			}
			mysqli_free_result($rs);

			// Query excerptional article count
			$sql = "SELECT COUNT(*) AS ex_article_count FROM bbs WHERE SID = " .
					$section["SID"] . " AND visible AND gen_ex";
			
			$rs = mysqli_query($db_conn, $sql);
			if ($rs == false)
			{
				echo mysqli_error($db_conn);
				return $result;
			}

			if ($row = mysqli_fetch_array($rs))
			{
				$result["ex_article_count"] = $row["ex_article_count"];
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
		echo "Query excerptional section list error: " . mysqli_error($db_conn);
		exit();
	}

	// Generate menu of excerptional section class
	$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M_ANNOUNCE
title       0, 0, "精华公布栏"
screen      2, 0, S_EGROUP

MENU;

	foreach ($ex_section_hierachy as $c_index => $section_class)
	{
		$display_row = ($c_index == 0 ? 4 : 0);

		$section_count = count($section_class["sections"]);

		$title_f = str_repeat(" ", 5 - intval(log10($section_count))) . $section_count . " ＋  " .
			$section_class['name'] . str_repeat(" ", 32 - strlen($section_class['name'])) .
			"[" . addslashes($section_class['title']) . "]";

		$buffer .= <<<MENU
		!M_EX_{$section_class['name']}   {$display_row}, 4, 1, 0,   "{$section_class['name']}",    "{$title_f}"

		MENU;
	}

	$buffer .= <<<MENU
%

MENU;

	foreach ($ex_section_hierachy as $c_index => $section_class)
	{
		// Generate menu of excerptional sections in section_class[$c_index]
		$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M_EX_{$section_class["name"]}
title       0, 0, "精华区 > {$section_class["title"]}"
screen      2, 0, S_EX_DIR

MENU;

		$display_row = 4;
		$class_title_f = "[" . addslashes($section_class['title']) . "]" . str_repeat(" ", 10 - str_length($section_class['title']));
		
		foreach ($section_class["sections"] as $s_index => $section)
		{
			$article_count = $section['udf_values']['ex_article_count'];
			if ($article_count == 0)
			{
				continue;
			}

			$title_f = str_repeat(" ", 5 - intval(log10($article_count))) . $article_count . " ＋  " .
				$section['name'] . str_repeat(" ", 20 - strlen($section['name'])) .
				$class_title_f . addslashes($section['title']) . str_repeat(" ", 22 - str_length($section['title'])) .
				(new DateTimeImmutable($section['udf_values']['ex_menu_tm']))->format("Y.m.d");

			$buffer .= <<<MENU
			@LIST_EX_SECTION   {$display_row}, 4, 1, 0,   "{$section['name']}",    "{$title_f}"

			MENU;

			$display_row = 0;
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
				($section['udf_values']['section_master'] == "" ? "诚征版主中" : $section['udf_values']['section_master']);

			$buffer .= <<<MENU
			@LIST_SECTION   {$display_row}, 4, 1, {$section['read_user_level']},   "{$section['name']}",    "{$title_f}"

			MENU;

			$display_row = 0;
		}
	}

	$buffer .= <<<MENU
%

MENU;

	// Generate menu of favorite sections
	$buffer .= <<<MENU
#---------------------------------------------------------------------
%menu M_FAVOR_SECTION
title       0, 0, "[版块收藏]"
screen      2, 0, S_BOARD
use_filter
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
				($section['udf_values']['section_master'] == "" ? "诚征版主中" : $section['udf_values']['section_master']);

			$buffer .= <<<MENU
			@LIST_SECTION   {$display_row}, 4, {$section['sid']}, {$section['read_user_level']},   "{$section['name']}",    "{$title_f}"

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
