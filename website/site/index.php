<?php
declare(strict_types=1);
require __DIR__ . '/lib.php';

$cfg    = mb_config();
$counts = mb_counts();
$total  = 0;
foreach ($counts as $perFile) {
    $total += array_sum($perFile);
}
header('Content-Type: text/html; charset=utf-8');
?><!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex, nofollow">
<title><?= h($cfg['title']) ?></title>
<link rel="icon" type="image/svg+xml" href="assets/mbchess.svg">
<link rel="stylesheet" href="assets/style.css">
</head>
<body>
<main>
  <header>
    <img src="assets/mbchess.svg" alt="" width="72" height="72">
    <div>
      <h1><?= h($cfg['title']) ?></h1>
      <p class="sub"><?= h($cfg['subtitle']) ?></p>
    </div>
  </header>

  <section class="notice">
    <strong>Entwicklerversion.</strong>
    Diese Builds sind Vorabversionen und ändern sich häufig. Es kann noch Fehler geben,
    Funktionen können sich ändern oder wegfallen. Jede Beta-Version läuft
    <strong>6 Wochen nach dem ersten Start</strong> ab. Danach einfach den aktuellen Build
    von dieser Seite holen.
  </section>

  <div class="cards">
  <?php foreach (MB_PLATFORMS as $key => $p):
      [$current, $older] = mb_split_current(mb_builds($key));
      $older = array_slice($older, 0, max(0, (int)$cfg['older_builds']));
      $sum   = array_sum($counts[$key] ?? []);
  ?>
    <section class="card">
      <h2><?= h($p['label']) ?></h2>
      <p class="hint"><?= h($p['hint']) ?></p>
      <?php if ($current): $first = $current[0]; ?>
        <p class="ver">
          <?= $first['version'] ? 'Version ' . h($first['version']) . ' · ' : '' ?>
          Stand <?= date('d.m.Y, H:i', $first['mtime']) ?> Uhr
        </p>
        <?php foreach ($current as $b): $sha = mb_checksum($b); ?>
          <div class="file">
            <a class="btn" href="download.php?p=<?= h($key) ?>&amp;f=<?= h(rawurlencode($b['name'])) ?>">
              Herunterladen<?= ($l = mb_label($b['name'])) !== '' ? ' · ' . h($l) : '' ?>
            </a>
            <dl>
              <dt>Datei</dt><dd><?= h($b['name']) ?></dd>
              <dt>Größe</dt><dd><?= mb_size($b['size']) ?></dd>
              <dt>Downloads</dt><dd><?= (int)($counts[$key][$b['name']] ?? 0) ?></dd>
              <?php if ($sha): ?><dt>SHA-256</dt><dd class="hash"><?= h($sha) ?></dd><?php endif; ?>
            </dl>
          </div>
        <?php endforeach; ?>
        <p class="sum">Downloads für <?= h($p['label']) ?> insgesamt: <?= (int)$sum ?></p>
        <?php if ($older): ?>
          <details>
            <summary>Frühere Builds</summary>
            <ul>
            <?php foreach ($older as $b): ?>
              <li>
                <a href="download.php?p=<?= h($key) ?>&amp;f=<?= h(rawurlencode($b['name'])) ?>"><?= h($b['name']) ?></a>
                <span><?= mb_size($b['size']) ?> · <?= date('d.m.Y', $b['mtime']) ?> · <?= (int)($counts[$key][$b['name']] ?? 0) ?> Downloads</span>
              </li>
            <?php endforeach; ?>
            </ul>
          </details>
        <?php endif; ?>
      <?php else: ?>
        <p class="empty">Noch kein Build verfügbar.</p>
      <?php endif; ?>
    </section>
  <?php endforeach; ?>
  </div>

  <footer>
    <p>Downloads insgesamt: <?= (int)$total ?></p>
    <?php if ($cfg['contact']): ?>
      <p>Fehler gefunden? Schreib an <a href="mailto:<?= h($cfg['contact']) ?>"><?= h($cfg['contact']) ?></a></p>
    <?php endif; ?>
    <?php if ($cfg['repo']): ?>
      <p><a href="<?= h($cfg['repo']) ?>">Quellcode auf GitHub</a> · GPL</p>
    <?php endif; ?>
    <p>© Michael Bergmann <?= date('Y') ?></p>
  </footer>
</main>
</body>
</html>
