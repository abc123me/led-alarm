<?php
$pid_fname="/var/run/led-alarm.pid";
$def_fname="/etc/led-alarm.conf";
$libcfgrw="libconfig-rw";
$old_prefix="old-";

require_once("libconfig-rw.php");

$default_cfg = new LibconfigRW("/etc/led-alarm.conf", TRUE, TRUE);      /* enable caching and use read-only mode */
$runtime_cfg = new LibconfigRW("/var/run/led-alarm.conf", TRUE, FALSE); /* enable caching, and use read-write mode */

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
enum ParseType {
	case Time;
	case Duration;
	case Color;
	case Int;
	case Verbosity;
	case NoiseTypes;
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
function html_to_config(string|null $val, LibconfigType $cfg, HtmlType $html): string|null {
	if($val === null)
		return null;
	if ($cfg !== LibconfigType::Int) {
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
function config_to_html(string|null $val, LibconfigType $cfg, HtmlType $html): string|null {
	if($val === null)
		return null;
	if ($cfg !== LibconfigType::Int) {
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
	public LibconfigType $configType;
	public ParseType $parseType;
	public string|null $defFieldName;
	public string|null $defCfgHtml;
	public bool $checkBox;

	public function __construct(int $fset, string $disp, HtmlType $htmlType, bool $checkBox, string $name, LibconfigType $configType, ParseType $parseType, string|null $defFieldName=null, string|null $defCfgHtml=null, string $html="") {
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
		global $default_cfg, $runtime_cfg, $old_prefix;

		$html = $this->html;
		$cval = null;
		$has_cfg = $runtime_cfg->get_auto($this->name, $cval);
		if(!$has_cfg)
			if(!$runtime_cfg->get_auto($old_prefix . $this->name, $cval))
				if(!($this->defFieldName !== null && $runtime_cfg->get_auto($this->defFieldName, $cval)))
					if(!$default_cfg->get_auto($this->name, $cval))
						if(!($this->defFieldName !== null && $default_cfg->get_auto($this->defFieldName, $cval)))
							error_log("Tried all the things but STILL failed to get a value for $this->name\n");

		if($this->checkBox)
			echo "<input type=\"checkbox\" name=\"enable-$this->name\"" . ($has_cfg ? " checked" : "") . "></input>";

		echo "<label for=\"" . $this->name . "\">" . $this->disp . ":</label>";

		if($cval !== null) {
			$value = config_to_html($cval, $this->configType, $this->htmlType);
			if($value !== null)
				$html.=" value=\"$value\"";
		}
		echo "<input type=\"" . $this->htmlType->value . "\" name=\"" . $this->name . "\" $html></input>";
		echo "</br>";
	}
	function handle_unchecked(array $post) {
		global $default_cfg, $runtime_cfg, $old_prefix;

		$ename = "enable-" . $this->name;
		if(array_has_string($post, $ename)) {
			if($post["$ename"] === "on") {
				return false;
			} else {
				error_log("Ignoring invalid value for $ename: " . $post["$ename"]);
				return true;
			}
		} else {
			$val = null;
			if($runtime_cfg->get_auto($this->name, $val)) {
				if($val !== null && !empty($val))
					$runtime_cfg->set_object($old_prefix . $this->name, $this->configType, $val);
				$runtime_cfg->delete($this->name);
				echo "$this->name: $val DELETE<br>";
			}
			return true;
		}
	}
	public function handle_post(array $post): bool {
		global $default_cfg, $runtime_cfg, $old_prefix;

		if(!array_has_string($post, $this->name))
			return false;

		if($this->checkBox)
			if($this->handle_unchecked($post))
				return true;

		$old_val = null;
		$new_val = html_to_config($post[$this->name], $this->configType, $this->htmlType);
		$runtime_cfg->get_auto($this->name, $old_val);
		if($new_val !== null && $new_val !== $old_val) {
			echo "$this->name: $old_val -> SET TO -> $new_val<br>";
			$runtime_cfg->set_object($this->name, $this->configType, $new_val);
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
	new Field(0, "Begin time",      HtmlType::Time,   FALSE, "normal-time",     LibconfigType::Int, ParseType::Time),
	new Field(0, "Ramp up time",    HtmlType::Number, FALSE, "ramp-up-time",    LibconfigType::Int, ParseType::Duration),
	new Field(0, "Keep on time",    HtmlType::Number, FALSE, "keep-on-time",    LibconfigType::Int, ParseType::Duration),
	new Field(0, "Override color",  HtmlType::Color,  TRUE,  "override-color",  LibconfigType::Int, ParseType::Color,      null, "16777215"),
	new Field(0, "Custom time",     HtmlType::Number, TRUE,  "override-time",   LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Sunday time",     HtmlType::Time,   TRUE,  "sunday-time",     LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Monday time",     HtmlType::Time,   TRUE,  "monday-time",     LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Tuesday time",    HtmlType::Time,   TRUE,  "tuesday-time",    LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Wednesday time",  HtmlType::Time,   TRUE,  "wednesday-time",  LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Thursday time",   HtmlType::Time,   TRUE,  "thursday-time",   LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Friday time",     HtmlType::Time,   TRUE,  "friday-time",     LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Saturday time",   HtmlType::Time,   TRUE,  "saturday-time",   LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(1, "Sunday time",     HtmlType::Time,   TRUE,  "sunday-time",     LibconfigType::Int, ParseType::Time,       "normal-time"),
	new Field(2, "Brightness",      HtmlType::Number, FALSE, "brightness",      LibconfigType::Int, ParseType::Int,        null, "255", "min=\"0\" max=\"255\""),
	new Field(2, "Fake time",       HtmlType::Number, TRUE,  "fake-time",       LibconfigType::Int, ParseType::Duration,   "normal-time"),
	new Field(2, "Fake day",        HtmlType::Number, TRUE,  "fake-day",        LibconfigType::Int, ParseType::Int,        null, "0"),
	new Field(2, "Verbosity",       HtmlType::Number, FALSE, "verbosity",       LibconfigType::Int, ParseType::Verbosity,  null, "0"),
	new Field(2, "Noise type",      HtmlType::Number, FALSE, "noise-type",      LibconfigType::Int, ParseType::NoiseTypes, null, "1"),
	new Field(2, "Noise intensity", HtmlType::Number, FALSE, "noise-intensity", LibconfigType::Int, ParseType::Int,        null, "0"),
	new Field(2, "Line correction", HtmlType::Number, FALSE, "line-fade",       LibconfigType::Int, ParseType::Int,        null, "5"),
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
			$cmd = "sudo /bin/kill -SIGUSR1 $pid 2>&1";
			if(exec($cmd, $out, $res) !== false) {
				echo "EXEC \"$cmd\" OK<br>";
			} else {
				$str = "Unknown error";
				if(is_array($out))
					$str = join("<br>", $out);
				echo "EXEC \"$cmd\" FAIL: $str<br>";
			}
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
	echo "<p>" . str_replace("\n", "<br>", file_get_contents("/var/run/led-alarm.conf")) . "</p>";
?>
</body>
</html>
