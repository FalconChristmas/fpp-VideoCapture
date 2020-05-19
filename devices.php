<?

$data = Array();

$devices = Array();
foreach( scandir("/dev/") as $file) {
    if (preg_match('/^video[0-9]*$/', $file)) {
        $info = Array();
        $info['name'] = '/dev/' . $file;

        $resolutions = Array();
        $rCount = 0;

        exec("v4l2-ctl --list-formats-ext -d /dev/$file 2>&1", $output, $return_val);
        if (($return_val == 0) &&
            (sizeof($output) > 0)) {
            for ($i = 0; $i < sizeof($output); $i++) {
                if (preg_match('/Size: Discrete/', $output[$i])) {
                    $size = preg_replace('/.*Size: Discrete /', '', $output[$i]);
                    $width = preg_replace('/x.*/', '', $size);
                    $height = preg_replace('/.*x/', '', $size);

                    $i++;
                    while (($i < sizeof($output)) && preg_match('/Interval: Discrete/', $output[$i])) {
                        $fps = preg_replace('/.*\(([0-9]*)\..*/', '$1', $output[$i]);

                        $k = sprintf( "%d:%d:%d", $width, $height, $fps);
                        $v = sprintf( "%s @ %d FPS", $size, $fps);
                        $resolutions[$k] = $v;
                        $i++;
                    }
                    if ($i <= sizeof($output)) {
                        $i--;
                    }
                }
            }
            $info['resolutions'] = $resolutions;
        } else {
            $info['Status'] = 'error, could not get resolutions';
        }
        unset($output);

        if (sizeof($info['resolutions']) > 0)
            $devices['/dev/' . $file] = $info;
    }
}

$data['devices'] = $devices;

header( "Content-Type: application/json");
header( "Access-Control-Allow-Origin: *");

echo json_encode($data);


?>
