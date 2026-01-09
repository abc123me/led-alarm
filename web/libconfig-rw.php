<?php

enum LibconfigType {
	case Bool;
	case Float;
	case Int;
	case Int64;
	case String;
}

class LibconfigRW {
	private string $cmd;
	private string $err;
	private string $res;
	private string $ret;

	public function __construct(string $fname, string $cmd="libconfig-rw") {
		$this->cmd = $cmd . " " . escapeshellarg($fname) . " ";
	}

	public function latest_err(): string|null {
		$err = $this->err;
		$this->err = null;
		return $err;
	}

	public function get_type(string $key): LibconfigType|null {
		return null;
	}

	public function get_int(string $key, int &$out): bool {
		return FALSE;
	}
	public function set_int(string $key, int &$out): bool {
		return FALSE;
	}
}

?>
