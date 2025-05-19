<?php
function load_section_list(array & $result, callable $filter, callable $udf_value_gen, mysqli $db_conn, array $filter_param = array()) : bool
{
	$sql = "SELECT SID, section_config.CID, section_config.sname, section_config.title AS s_title,
			section_config.comment, section_class.cname, section_class.title AS c_title,
			section_config.recommend, section_config.read_user_level, section_config.write_user_level
			FROM section_config INNER JOIN section_class ON section_config.CID = section_class.CID
			WHERE section_class.enable AND section_config.enable
			ORDER BY section_class.sort_order, section_config.sort_order";

	$rs = mysqli_query($db_conn, $sql);
	if ($rs == false)
	{
        return false;
	}

	$last_cid = -1;
	$last_c_name = "";
	$last_c_title = "";
	$section_list = array();
	while ($row = mysqli_fetch_array($rs))
	{
		if ($row["CID"] != $last_cid)
		{
			if (count($section_list) > 0)
			{
				array_push($result, array(
					"cid" => $last_cid,
					"name" => $last_c_name,
					"title" => $last_c_title,
					"sections" => $section_list,
				));

				$section_list = array();
			}

			$last_cid = $row["CID"];
			$last_c_name = $row["cname"];
			$last_c_title = $row["c_title"];
		}

        if (call_user_func($filter, $row, $filter_param))
		{
			array_push($section_list, array(
				"sid" => $row["SID"],
				"name" => $row["sname"],
				"title" => $row["s_title"],
				"comment" => $row["comment"],
				"read_user_level" => $row["read_user_level"],
				"write_user_level" => $row["write_user_level"],
				"udf_values" => call_user_func($udf_value_gen, $row, $filter_param),
			));
		}
	}

	if (count($section_list) > 0)
	{
		array_push($result, array(
			"cid" => $last_cid,
			"name" => $last_c_name,
			"title" => $last_c_title,
			"sections" => $section_list,
		));
	}

	mysqli_free_result($rs);

    return true;
}
