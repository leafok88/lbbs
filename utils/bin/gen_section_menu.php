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
			return null;
		},
		$db_conn);

	if ($ret == false)
	{
		echo "Query section list error: " . mysqli_error($db_conn);
		exit();
	}

	// Generate menu of section class
	$buffer .= <<<MENU
#---------------------------------------------------------------------
%S_EGROUP
    ·µ»Ø[[1;32m¡û[0;37m] ½øÈë[[1;32m¡ú[0;37m] Ñ¡Ôñ[[1;32m¡ü[0;37m,[1;32m¡ý[0;37m]
[44;37m    [1;37mÏÂÊô°æ¿é  À¸Ä¿Ãû³Æ                    ÖÐ  ÎÄ  Ðð  Êö                        [m




















%
#---------------------------------------------------------------------
%menu M_EGROUP
title       0, 0, "[À¸Ä¿ÁÐ±í]"
screen      2, 0, S_EGROUP

MENU;

	foreach ($section_hierachy as $c_index => $section_class)
	{
		$display_row = ($c_index == 0 ? 4 : 0);

		$section_count = count($section_class["sections"]);

		$title_f = str_repeat(" ", 5 - intval(log10($section_count))) . $section_count . " £«  " .
			$section_class['name'] . str_repeat(" ", 28 - strlen($section_class['name'])) .
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
%S__{$section_class["name"]}
    ·µ»Ø[[1;32m¡û[0;37m] ½øÈë[[1;32m¡ú[0;37m] Ñ¡Ôñ[[1;32m¡ü[0;37m,[1;32m¡ý[0;37m]
[44;37m    [1;37mÖ÷ÌâÊýÁ¿  °æ¿éÃû³Æ                    ÖÐ  ÎÄ  Ðð  Êö                        [m




















%
#---------------------------------------------------------------------
%menu M__{$section_class["name"]}
title       0, 0, "[{$section_class["title"]}]"
screen      2, 0, S__{$section_class["name"]}

MENU;

		$class_title_f = "[" . addslashes($section_class['title']) . "]" . str_repeat(" ", 14 - str_length($section_class['title']));

		foreach ($section_class["sections"] as $s_index => $section)
		{
			$display_row = ($s_index == 0 ? 4 : 0);

			$topic_count = 0; // TODO

			$title_f = str_repeat(" ", 5 - intval(log10($topic_count))) . $topic_count . " £«  " .
				$section['name'] . str_repeat(" ", 22 - strlen($section['name'])) .
				$class_title_f .
				addslashes($section['title']);

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
%S_BOARD
    ·µ»Ø[[1;32m¡û[0;37m] ½øÈë[[1;32m¡ú[0;37m] Ñ¡Ôñ[[1;32m¡ü[0;37m,[1;32m¡ý[0;37m]
[44;37m    [1;37mÏÂÊô°æ¿é  À¸Ä¿Ãû³Æ                    ÖÐ  ÎÄ  Ðð  Êö                        [m




















%
#---------------------------------------------------------------------
%menu M_BOARD
title       0, 0, "[°æ¿éÁÐ±í]"
screen      2, 0, S_BOARD
page        4, 1, 20

MENU;

	$display_row = 4;

	foreach ($section_hierachy as $c_index => $section_class)
	{
		$class_title_f = "[" . addslashes($section_class['title']) . "]" . str_repeat(" ", 14 - str_length($section_class['title']));

		foreach ($section_class["sections"] as $s_index => $section)
		{
			$topic_count = 0; // TODO

			$title_f = str_repeat(" ", 5 - intval(log10($topic_count))) . $topic_count . " £«  " .
				$section['name'] . str_repeat(" ", 22 - strlen($section['name'])) .
				$class_title_f .
				addslashes($section['title']);

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
