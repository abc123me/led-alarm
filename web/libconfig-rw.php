<?php

enum LibconfigType: string {
	case Bool = "bool";
	case Float = "float";
	case Int = "int";
	case Int64 = "int64";
	case String = "string";
	case Auto = "auto";
}

class LibconfigRW {
	private array|null $cache;
	private string $cmd;
	private string $err;
	private string $res;
	private string $ret;
	private bool $ronly;
	private bool $dry_run;

	private function run_libconfig(string ...$args): bool|string {
		$res = false;
		$out = array();
		$cmd = escapeshellcmd($this->cmd) . " ";
		foreach ($args as $arg) {
			if(!is_string($arg)) {
				$this->err = "LibconfigRW: run_libconfig argument isn't a string";
				return false;
			} else $cmd .= escapeshellarg($arg) . " ";
		}

		$ret = exec($cmd . "2>&1", $out, $res);
		if($res === 0) {
			//error_log("OK   $cmd -> $ret\n");
			return $ret;
		} else {
			$str = "$cmd executed but with no/unknown output";
			if(is_array($out))
				$str = join("<br>", $out);
			$this->err = $str;
			//error_log("FAIL $cmd -> $str\n");
			return false;
		}
	}

	public function __construct(string $fname, bool $cache, bool $ro, bool $dry_run=FALSE, string $cmd="libconfig-rw") {
		$this->cmd = $cmd . " " . escapeshellarg($fname) . " ";
		$this->cache = $cache ? array() : null;
		$this->dry_run = $dry_run;
		$this->ronly = $ro;
	}

	public function get_latest_err(): string|null {
		$err = $this->err;
		$this->err = null;
		return $err;
	}

	public function get_type(string $key): LibconfigType|null {
		$res = $this->run_libconfig("type", $key);
		foreach(LibconfigType::cases() as $type)
			if($type->value === "$res")
				return $type;
		return null;
	}
	public function delete(string $key): bool {
		if($this->ronly) {
			$this->err = "LibconfigRW: object is marked as read only!";
			return false;
		}
		if($this->dry_run) {
			return true;
		}
		return $this->run_libconfig("delete", $key);
	}

	public function get_object(string $key, LibconfigType $type, string|null &$out): bool {
		$res = $this->run_libconfig("read", $type->value, $key);
		if($res !== false) {
			$out = $res;
			return true;
		} else return false;
	}
	public function set_object(string $key, LibconfigType $type, string $val): bool {
		if($this->ronly) {
			$this->err = "LibconfigRW: object is marked as read only!";
			return false;
		}
		if($this->dry_run) {
			return true;
		}
		$res = $this->run_libconfig("write", $type->value, $key, $val);
		return $res !== false;
	}

	public function get_int(string $key, int|null &$out): bool {
		$res = null;
		if($this->get_object($key, LibconfigType::Int, $res)) {
			$out = intval($res);
			return TRUE;
		} else return FALSE;
	}
	public function set_int(string $key, int $val): bool {
		return $this->set_object($key, LibconfigType::Int, strval($val));
	}
	public function get_float(string $key, float|null &$out): bool {
		$res = null;
		if($this->get_object($key, LibconfigType::Float, $res)) {
			$out = intval($res);
			return TRUE;
		} else return FALSE;
	}
	public function set_float(string $key, float $val): bool {
		return $this->set_object($key, LibconfigType::Float, strval($val));
	}
	public function get_bool(string $key, bool|null &$out): bool {
		$res = null;
		if($this->get_object($key, LibconfigType::Bool, $res)) {
			$out = $res === "true";
			return TRUE;
		} else return FALSE;
	}
	public function set_bool(string $key, bool $val): bool {
		return $this->set_object($key, LibconfigType::Bool, $val ? "true" : "false");
	}
	public function get_string(string $key, string|null &$out): bool {
		$res = null;
		if($this->get_object($key, LibconfigType::String, $res)) {
			$out = $res;
			return TRUE;
		} else return FALSE;
	}
	public function set_string(string $key, string $val): bool {
		return $this->set_object($key, LibconfigType::String, $val);
	}
	public function get_auto(string $key, string|null &$out): bool {
		$res = null;
		if($this->get_object($key, LibconfigType::Auto, $res)) {
			$out = $res;
			return TRUE;
		} else return FALSE;
	}
	public function set_auto(string $key, string $val): bool {
		return $this->set_object($key, LibconfigType::Auto, $val);
	}
}

?>
