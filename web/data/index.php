<?php
require('secrets.php');

$filename = 'temperature.dat';

function returnResult($success, $message) {
        $result = array('success' => $success, 'message' => $message);
        die(json_encode($result));
}

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
        $contents=file_get_contents($filename);
        $contents=rtrim($contents,",");
        print("const data=[".$contents."];");
} else if ($_SERVER['REQUEST_METHOD'] == 'POST' ) {
        if (!isset($_SERVER['HTTP_X_AUTH_TOKEN']) || sha1($_SERVER['HTTP_X_AUTH_TOKEN']) != $sha1_of_auth) {
                returnResult(false, 'not authorized');
        }
        $requestBody = file_get_contents('php://input');
        error_log($requestBody);
        $data = json_decode($requestBody);
        if (isset($data->reset) && $data->reset) {
                file_put_contents($filename, "");
                returnResult(true,'reset');
        }
        if (!isset($data->temperature) || !isset($data->humidity)) {
                returnResult(false,'empty data');
        }
        if (empty($data->temperature) || empty($data->humidity) || ($data->temperature < 1 && $data->humidity < 1)) {
                returnResult(false,'invalid data');
        }
        $line = "[".time().",".$data->temperature.",".$data->humidity."],\n";
        file_put_contents($filename, $line, FILE_APPEND );
        returnResult(true, 'added data');
}
?>
