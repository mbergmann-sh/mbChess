<?php
declare(strict_types=1);
require __DIR__ . '/lib.php';

$platform = (string)($_GET['p'] ?? '');
$name     = basename((string)($_GET['f'] ?? ''));

$build = null;
foreach (mb_builds($platform) as $b) {
    if ($b['name'] === $name) {
        $build = $b;
        break;
    }
}

if ($build === null) {
    http_response_code(404);
    header('Content-Type: text/plain; charset=utf-8');
    echo "Datei nicht gefunden.\n";
    exit;
}

// Nur echte Downloads zählen (keine HEAD-Anfragen von Link-Vorschauen)
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    mb_count_download($platform, $build['name']);
}

while (ob_get_level() > 0) {
    ob_end_clean();
}
@set_time_limit(0);
header('Content-Type: application/octet-stream');
header('Content-Disposition: attachment; filename="' . str_replace('"', '', $build['name']) . '"');
header('Content-Length: ' . $build['size']);
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');
if ($_SERVER['REQUEST_METHOD'] !== 'HEAD') {
    readfile($build['path']);
}
