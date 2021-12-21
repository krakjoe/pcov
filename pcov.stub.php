<?php

/** @generate-function-entries */

namespace pcov;

function start(): void {}

function stop(): void {}

function collect(int $type=\pcov\all, array $filter = []): ?array {}

function clear(bool $files = false): void {}

function waiting(): ?array {}

function memory(): ?int {}

