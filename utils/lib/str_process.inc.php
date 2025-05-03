<?php
function str_length(string $str) : string
{
	$len = strlen($str);
	$ret = 0;

	for ($i = 0; $i < $len; $i++)
	{
		$c = $str[$i];

		// Process GBK Chinese characters
		$v1 = ord($c);
		if ($v1 > 127 && $v1 <= 255) // GBK chinese character
		{
			$i++;
			$c .= $str[$i];

			$ret += 2;
		}
		else
		{
			$ret++;
		}
	}

	return $ret;
}

function split_line(string $str, string $prefix = "", int $width = 76, int $lines_limit = PHP_INT_MAX, string $end_of_line = "\n") : string
{
	if ($width <= 0)
	{
		return $str;
	}

	$result = "";
	$len = strlen($str);
	$prefix_len = str_length($prefix);

	$lines_count = 0;

	$line = $prefix;
	$line_len = $prefix_len;
	for ($i = 0; $i < $len && $lines_count < $lines_limit; $i++)
	{
		$c = $str[$i];

		// Skip special characters
		if ($c == "\r" || $c == "\7")
		{
			continue;
		}

		if ($c == "\n")
		{
			if ($lines_count + 1 >= $lines_limit)
			{
				break;
			}

			$result .= ($line . $end_of_line);
			$lines_count++;
			$line = $prefix;
			$line_len = $prefix_len;
			continue;
		}

		// Process GBK Chinese characters
		$v1 = ord($c);
		if ($v1 > 127 && $v1 <= 255) // GBK chinese character
		{
			$i++;
			$c .= $str[$i];

			// Each GBK CJK character should use two character length for display
			if ($line_len + 2 > $width)
			{
				if ($lines_count + 1 >= $lines_limit)
				{
					break;
				}

				$result .= ($line . $end_of_line);
				$lines_count++;
				$line = $prefix;
				$line_len = $prefix_len;
			}
			$line_len += 2;
		}
		else
		{
			$line_len++;
		}

		if ($line_len > $width)
		{
			if ($lines_count + 1 >= $lines_limit)
			{
				break;
			}

			$result .= ($line . $end_of_line);
			$lines_count++;
			$line = $prefix;
			$line_len = $prefix_len + 1;
		}

		$line .= $c;
	}

	if ($lines_count < $lines_limit)
	{
		$result .= $line;
	}

	return $result;
}
