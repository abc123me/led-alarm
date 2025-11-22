<!DOCTYPE html>
<html><head>
	<title>LED Alarm clock</title>
</head><body>
	<form action="/index.php" method="GET">
		<fieldset>
			<label for="btime">Default time:</label>
			<input type="time" name="btime"></input>
			</br>
			<label for="ramp">Ramp up time:</label>
			<input type="number" name="ramp"></input>
			</br>
			<label for="keepon">Keep on time:</label>
			<input type="number" name="keep"></input>
			</br>
			<label for="brightness">Brightness:</label>
			<input type="number" name="brightness"></input>
			</br>
			<input type="checkbox" name="ccolor"></input>
			<label for="ocolor">Custom color:</label>
			<input type="color" name="ocolor"></input>
		</fieldset>
		<fieldset>
<?php
			$arr = array("sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday");
			foreach ($arr as $day) {
				$name = $day;
				echo "<input type=\"checkbox\" name=\"${name}_en\"></input>";
				echo "<label for=\"checkbox\" name=\"${name}\">" . ucfirst($day) . " time: </input>";
				echo "<input type=\"time\" name=\"${name}\"></input></br>";
			}
?>
		</fieldset>
		<input type="submit" value="Submit"></input>
	</form>
</body></html>
