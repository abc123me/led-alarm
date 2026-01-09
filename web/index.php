<?php
$cfg_fname="/var/run/led-alarm.conf";
$pid_fname="/var/run/led-alarm.pid";
$def_fname="/etc/led-alarm.conf";
$libcfgrw="libconfig-rw";
$old_prefix="old-";
$dry_run=false;

//require_once("libconfig-rw.php");

enum Verbosity: int {
	case Info    = 0;
	case Verbose = 1;
	case Debug   = 2;
}
enum NoiseTypes: int {
	case Off     = 0;
	case Random  = 1;
	case Sine    = 2;
	case Clouds  = 3;
}
enum HtmlType: string {
	case Time    = "time";
	case Number  = "number";
	case Color   = "color";
}
enum ConfigType: string {
	case Int     = "int";
	case Int64   = "int64";
	case Float   = "float";
	case Boolean = "bool";
	case String  = "string";
}
enum ParseType {
	case Time;
	case Duration;
	case Color;
	case Int;
	case Verbosity;
	case NoiseTypes;
}

function get_config_type(string $key): string|null {
	global $cfg_fname, $libcfgrw;

	$res = false;
	$out = array();
	$ret = exec("$libcfgrw " . escapeshellcmd($cfg_fname) . " type " . escapeshellcmd($key) . " 2>&1", $out, $res);
	//if($res !== 0)
	//	error_log("failed to read type of $key from $cfg_fname - err: " . join("\n", $out));
	if($res !== 0 || $ret === false)
		return null;
	else
		return $ret;
}
function get_config(string $key, string|null $def=null, string|null $type=null): string|null {
	global $cfg_fname, $libcfgrw;

	$res = false;
	$out = array();

	if($type === NULL)
		$type = "auto";

	$ret = exec("$libcfgrw " . escapeshellcmd($cfg_fname) . " read " . escapeshellcmd($type) . " " . escapeshellcmd($key) . " 2>&1", $out, $res);
	//if($res !== 0)
	//	error_log("failed to read $key from $cfg_fname - err: " . join("\n", $out));
	if($res !== 0 || $ret === false)
		return null;
	else
		return $ret;
}
function set_config(string $key, string $val, string|null $type=null): bool {
	global $cfg_fname, $libcfgrw, $dry_run;

	if($dry_run === true)
		return true;

	$res = false;
	$out = false;

	if($type === null)
		$type = "auto";

	$ret = exec("$libcfgrw " . escapeshellcmd($cfg_fname) . " write " . escapeshellcmd($type) . " " . escapeshellcmd($key) . " " . escapeshellcmd($val) . " 2>&1", $out, $res);
	return $ret !== false && $res === 0;
}
function del_config(string $key): bool {
	global $cfg_fname, $libcfgrw, $dry_run;

	if($dry_run === true)
		return true;

	$res = false;
	$out = false;
	$ret = exec("$libcfgrw " . escapeshellcmd($cfg_fname) . " delete " . escapeshellcmd($key) . "2>&1", $out, $res);
	return $ret !== false && $res === 0;
}

function array_has_string(array $post, string $key): bool {
	if(!array_key_exists($key, $post))
		return false;
	if(!isset($post[$key]))
		return false;
	if(empty($post[$key]))
		return false;
	if(!is_string($post[$key]))
		return false;
	return true;
}
function get_config_value(string $name, bool $useOldIfUnset=true): string|null {
	global $old_prefix;
	$value = get_config($name);
	if($value !== null) {
		return $value;
	} else if($useOldIfUnset) {
		$value = get_config($old_prefix . $name);
		if($value !== null) {
			return $value;
		}
	}
	return null;
}
function html_to_config(string|null $val, ConfigType $cfg, HtmlType $html): string|null {
	if($val === null)
		return null;
	if ($cfg !== ConfigType::Int) {
		error_log("Config type is not an integer: $cfg\n");
		return null;
	}
	$val = strtolower(trim($val));
	switch($html) {
		case HtmlType::Time:
			if(str_contains($val, ":")) {
				$exp = explode(":", $val);
				if(count($exp) === 2) {
					$mins = 0;
					$hrs = intval($exp[0]);
					$exp2 = explode(" ", $exp[1]);
					if(count($exp2) < 1) {
						error_log("Invalid time given: $val\n");
					} else {
						$mins = intval($exp2[0]);
						if(str_contains($val, "pm"))
							$hrs += 12;
					}
					return strval($mins + $hrs * 60);
				} else {
					error_log("Invalid time given: $val\n");
					return null;
				}
			}
		case HtmlType::Number:
			if(is_numeric($val)) {
				return $val;
			} else {
				error_log("Input value is not numeric!\n");
				return null;
			}
		case HtmlType::Color:
			if(preg_match("/^#[0-9a-f]{1,6}$/", $val)) {
				return hexdec(trim(ltrim($val, "#")));
			} else {
				error_log("Input value doesn't match color regex!\n");
				return null;
			}
		default:
			error_log("Invalid html type: $html\n");
			return null;
	}
}
function config_to_html(string|null $val, ConfigType $cfg, HtmlType $html): string|null {
	if($val === null)
		return null;
	if ($cfg !== ConfigType::Int) {
		error_log("Config type is not an integer: $cfg\n");
		return null;
	}
	$val = trim($val);
	if (!is_numeric($val)) {
		error_log("Config value is not numeric: $val\n");
		return null;
	}
	$val = intval($val);
	switch($html) {
		case HtmlType::Time:
			$hrs = $val / 60;
			$mins = $val % 60;
			return sprintf("%02d:%02d", $hrs, $mins);
		case HtmlType::Number:
			return $val;
		case HtmlType::Color:
			return sprintf("#%06X", $val);
		default:
			error_log("Invalid html type: $html\n");
			return null;
	}
}


class Field {
	public int $fset;
	public string $disp;
	public string $name;
	public HtmlType $htmlType;
	public ConfigType $configType;
	public ParseType $parseType;
	public string|null $defFieldName;
	public string|null $defCfgHtml;
	public bool $checkBox;

	public function __construct(int $fset, string $disp, HtmlType $htmlType, bool $checkBox, string $name, ConfigType $configType, ParseType $parseType, string|null $defFieldName=null, string|null $defCfgHtml=null, string $html="") {
		$this->fset = $fset;
		$this->disp = $disp;
		$this->name = $name;
		$this->htmlType = $htmlType;
		$this->configType = $configType;
		$this->parseType = $parseType;
		$this->checkBox = $checkBox;
		$this->defFieldName = $defFieldName;
		$this->defCfgHtml = $defCfgHtml;
		$this->html = $html;
	}

	public function print_form_input() {
		if($this->checkBox) {
			if(get_config_type($this->name) !== null)
				echo "<input type=\"checkbox\" name=\"enable-" . $this->name . "\" checked></input>";
			else
				echo "<input type=\"checkbox\" name=\"enable-" . $this->name . "\"></input>";
		}

		echo "<label for=\"" . $this->name . "\">" . $this->disp . ":</label>";

		$html = $this->html;
		$cval = get_config($this->name);
		if($cval === null && $this->defFieldName !== null)
			$cval = get_config($this->defFieldName);
		if($cval === null && $this->defCfgHtml !== null)
			$cval = $this->defCfgHtml;
		$value = config_to_html($cval, $this->configType, $this->htmlType);
		if($value !== null)
			$html.=" value=\"$value\"";
		echo "<input type=\"" . $this->htmlType->value . "\" name=\"" . $this->name . "\" $html></input>";
		echo "</br>";
	}
	function handle_unchecked(array $post) {
		global $old_prefix;
		$ename = "enable-" . $this->name;
		if(array_has_string($post, $ename)) {
			if($post["$ename"] === "on") {
				return false;
			} else {
				error_log("Ignoring invalid value for $ename: " . $post["$ename"]);
				return true;
			}
		} else {
			$type = $this->configType->value;
			if(get_config_type($this->name) !== null) {
				$val = get_config($this->name, null, $type);
				set_config($old_prefix . $this->name, $val, $type);
				del_config($this->name);
				echo "$this->name: $val DELETE<br>";
			}
			return true;
		}
	}
	public function handle_post(array $post): bool {
		$name = $this->name;

		if(!array_has_string($post, $name))
			return false;

		if($this->checkBox)
			if($this->handle_unchecked($post))
				return true;

		$cval = get_config($name);
		$pval = html_to_config($post[$name], $this->configType, $this->htmlType);
		if($pval !== null && $pval !== $cval) {
			echo "$name: $cval -> SET TO -> $pval<br>";
			set_config($this->name, $pval, $this->configType->value);
		}

		return true;
	}
}

$fieldsets = array(
	"Normal options",
	"Custom schedule",
	"Debug options"
);
$fields = array(
	/* field set, display name,     html type,        chkbx, config name,              raw type, parse type, default fname, defualt html, extra html */
	new Field(0, "Begin time",      HtmlType::Time,   FALSE, "normal-time",     ConfigType::Int, ParseType::Time),
	new Field(0, "Ramp up time",    HtmlType::Number, FALSE, "ramp-up-time",    ConfigType::Int, ParseType::Duration),
	new Field(0, "Keep on time",    HtmlType::Number, FALSE, "keep-on-time",    ConfigType::Int, ParseType::Duration),
	new Field(0, "Override color",  HtmlType::Color,  TRUE,  "override-color",  ConfigType::Int, ParseType::Color,      null, "16777215"),
	new Field(0, "Custom time",     HtmlType::Number, TRUE,  "override-time",   ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Sunday time",     HtmlType::Time,   TRUE,  "sunday-time",     ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Monday time",     HtmlType::Time,   TRUE,  "monday-time",     ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Tuesday time",    HtmlType::Time,   TRUE,  "tuesday-time",    ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Wednesday time",  HtmlType::Time,   TRUE,  "wednesday-time",  ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Thursday time",   HtmlType::Time,   TRUE,  "thursday-time",   ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Friday time",     HtmlType::Time,   TRUE,  "friday-time",     ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Saturday time",   HtmlType::Time,   TRUE,  "saturday-time",   ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Sunday time",     HtmlType::Time,   TRUE,  "sunday-time",     ConfigType::Int, ParseType::Time,       "normal-time"),
	new Field(2, "Brightness",      HtmlType::Number, FALSE, "brightness",      ConfigType::Int, ParseType::Int,        null, "255", "min=\"0\" max=\"255\""),
	new Field(2, "Fake time",       HtmlType::Time,   TRUE,  "fake-time",       ConfigType::Int, ParseType::Duration,   "normal-time"),
	new Field(2, "Fake day",        HtmlType::Number, TRUE,  "fake-day",        ConfigType::Int, ParseType::Int,        null, "0"),
	new Field(2, "Verbosity",       HtmlType::Number, FALSE, "verbosity",       ConfigType::Int, ParseType::Verbosity,  null, "0"),
	new Field(2, "Noise type",      HtmlType::Number, FALSE, "noise-type",      ConfigType::Int, ParseType::NoiseTypes, null, "1"),
	new Field(2, "Noise intensity", HtmlType::Number, FALSE, "noise-intensity", ConfigType::Int, ParseType::Int,        null, "0"),
	new Field(2, "Line correction", HtmlType::Number, FALSE, "line-fade",       ConfigType::Int, ParseType::Int,        null, "5"),
);

?>

<!DOCTYPE html>
<html>
<head>
	<title>LED Alarm clock</title>
	<link rel="stylesheet" href="styles.css">
</head>
<body>
	<h1>LED Alarm clock<h1>

<?php
	$pid = file_get_contents($pid_fname);
	if($pid !== false && file_exists("/proc/$pid")) {
		echo "<h2>LED Alarm clock daemon (pid=$pid) is active!</h2>";
	} else {
		echo "<h2>LED Alarm clock daemon (pid=$pid) is inactive!</h2>";
	}

	if(isset($_POST)) {
		echo "<p>";
		$changes = false;
		foreach($fields as $field)
			$changes |= $field->handle_post($_POST);
		if($changes) {
			$res = false;
			$out = array();
			$cmd = "sudo kill -SIGUSR1 $pid 2>&1";
			$ret = exec($cmd, $out, $res);
			if($ret !== 0 || res !== false) {
				$str = "Unknwon error";
				if(is_array($out))
					$str = join("<br>", $out);
				echo "EXEC \"$cmd\" FAIL: $str<br>";
			} else echo "EXEC \"$cmd\" OK<br>";

		}
		echo "</p>";
	}
?>

	<form action="/index.php" method="POST"> <?php
		foreach($fieldsets as $num => $fset) {
				echo "<fieldset>";
				echo "<h3>" . $fset . "</h3>";
				foreach($fields as $field)
					if($field->fset == $num)
						$field->print_form_input();
				echo "</fieldset>";
		} ?>
		<input type="submit" value="Submit"></input>
	</form>

<?php
	echo "<br><h3>Config file:</h3>";
	echo "<p>" . str_replace("\n", "<br>", file_get_contents($cfg_fname)) . "</p>";
?>
</body>
</html>
