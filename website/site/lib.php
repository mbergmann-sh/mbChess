<?php
declare(strict_types=1);

const MB_PLATFORMS = [
    'windows' => ['label' => 'Windows', 'hint' => 'Windows 10 / 11, 64 Bit'],
    'linux'   => ['label' => 'Linux',   'hint' => 'Debian 12+, Ubuntu und Derivate, 64 Bit, mit Qt 6 aus der Distribution'],
];

function mb_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        $cfg = require __DIR__ . '/config.php';
    }
    return $cfg;
}

function mb_data_file(string $name): string
{
    return __DIR__ . '/data/' . $name;
}

function mb_has_allowed_ext(string $file): bool
{
    $lower = strtolower($file);
    foreach (mb_config()['extensions'] as $ext) {
        if (str_ends_with($lower, '.' . strtolower($ext))) {
            return true;
        }
    }
    return false;
}

/** Alle Builds einer Plattform, neueste zuerst. */
function mb_builds(string $platform): array
{
    $dir = __DIR__ . '/builds/' . $platform;
    if (!isset(MB_PLATFORMS[$platform]) || !is_dir($dir)) {
        return [];
    }
    $list = [];
    foreach (scandir($dir) ?: [] as $f) {
        if ($f[0] === '.' || !mb_has_allowed_ext($f)) {
            continue;
        }
        $path = $dir . '/' . $f;
        if (!is_file($path)) {
            continue;
        }
        $list[] = [
            'name'    => $f,
            'path'    => $path,
            'size'    => filesize($path),
            'mtime'   => filemtime($path),
            'version' => preg_match('/(\d+\.\d+\.\d+)/', $f, $m) ? $m[1] : '',
        ];
    }
    usort($list, fn($a, $b) => $b['mtime'] <=> $a['mtime'] ?: strcmp($b['name'], $a['name']));
    return $list;
}

/** Liest/ändert eine JSON-Datei unter exklusiver Sperre. */
function mb_json_update(string $file, ?callable $change): array
{
    $fh = @fopen($file, 'c+');
    if ($fh === false) {
        return [];
    }
    flock($fh, $change ? LOCK_EX : LOCK_SH);
    $raw  = stream_get_contents($fh);
    $data = json_decode($raw ?: '{}', true);
    if (!is_array($data)) {
        $data = [];
    }
    if ($change) {
        $data = $change($data);
        ftruncate($fh, 0);
        rewind($fh);
        fwrite($fh, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE));
        fflush($fh);
    }
    flock($fh, LOCK_UN);
    fclose($fh);
    return $data;
}

function mb_counts(): array
{
    return mb_json_update(mb_data_file('counts.json'), null);
}

function mb_count_download(string $platform, string $name): void
{
    mb_json_update(mb_data_file('counts.json'), function (array $d) use ($platform, $name) {
        $d[$platform][$name] = ($d[$platform][$name] ?? 0) + 1;
        return $d;
    });
}

function mb_checksum(array $build): string
{
    if (!mb_config()['checksums']) {
        return '';
    }
    $key   = $build['name'] . '|' . $build['size'] . '|' . $build['mtime'];
    $cache = mb_json_update(mb_data_file('checksums.json'), null);
    if (isset($cache[$key])) {
        return $cache[$key];
    }
    $hash = hash_file('sha256', $build['path']) ?: '';
    mb_json_update(mb_data_file('checksums.json'), function (array $d) use ($key, $hash) {
        $d[$key] = $hash;
        return $d;
    });
    return $hash;
}

/** Teilt die Builds in die neueste Version (alle Dateien davon) und den Rest. */
function mb_split_current(array $builds): array
{
    if (!$builds) {
        return [[], []];
    }
    // Same version as the newest file; files without a version in the name
    // (fixed names like mbChess-Setup-de.exe) are always current.
    $ver = $builds[0]['version'];
    $current = $older = [];
    foreach ($builds as $b) {
        if ($b['version'] === $ver) {
            $current[] = $b;
        } else {
            $older[] = $b;
        }
    }
    usort($current, fn($a, $b) => strcmp($a['name'], $b['name']));
    return [$current, $older];
}

/** Kurzer Name für den Download-Knopf, aus dem Dateinamen. */
function mb_label(string $name): string
{
    $n = strtolower($name);
    $lang = preg_match('/[-_]de[-_.]/', $n) ? 'Deutsch' : (preg_match('/[-_]en[-_.]/', $n) ? 'English' : '');
    if (str_ends_with($n, '.exe') || str_ends_with($n, '.msi')) {
        return $lang !== '' ? 'Installer ' . $lang : 'Installer';
    }
    foreach (['.appimage' => 'AppImage', '.deb' => 'Debian-Paket', '.tar.gz' => 'Archiv (.tar.gz)',
              '.tar.xz' => 'Archiv (.tar.xz)', '.tgz' => 'Archiv (.tgz)', '.zip' => 'Zip-Archiv', '.7z' => '7-Zip-Archiv'] as $ext => $l) {
        if (str_ends_with($n, $ext)) {
            return $lang !== '' ? $l . ' ' . $lang : $l;
        }
    }
    return $lang;
}

function mb_size(int $bytes): string
{
    if ($bytes >= 1048576) {
        return number_format($bytes / 1048576, 1, ',', '.') . ' MB';
    }
    return number_format(max(1, $bytes / 1024), 0, ',', '.') . ' KB';
}

function h(string $s): string
{
    return htmlspecialchars($s, ENT_QUOTES, 'UTF-8');
}

mb_config(); // lädt config.php (setzt auch die Zeitzone)
