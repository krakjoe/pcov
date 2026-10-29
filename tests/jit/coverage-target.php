<?php

function pcov_jit_implicit_return(int $value)
{
    $value++;
}

function pcov_jit_ternary(bool $condition): int
{
    return $condition
        ? 1
        : 2;
}

function pcov_jit_loop(array $values): int
{
    $sum = 0;
    foreach ($values as $value) {
        if ($value < 0) {
            continue;
        }
        $sum += $value;
    }
    return $sum;
}

function pcov_jit_match(int $value): string
{
    return match ($value) {
        1 => 'one',
        2 => 'two',
        default => 'other',
    };
}

function pcov_jit_exception(bool $throw): string
{
    try {
        if ($throw) {
            throw new RuntimeException('jit');
        }
        return 'ok';
    } catch (RuntimeException $exception) {
        return $exception->getMessage();
    } finally {
        $finished = true;
    }
}

function pcov_jit_generator(): Generator
{
    yield 1;
    yield from [2, 3];
}

function pcov_jit_recursive(int $value): int
{
    if ($value === 0) {
        return 0;
    }
    return $value + pcov_jit_recursive($value - 1);
}

function pcov_jit_side_exit(mixed $value): int
{
    if (is_int($value)) {
        return $value + 1;
    }
    return strlen($value);
}

function pcov_jit_stopped_only(): void
{
    $notCovered = true;
}

/** A public schema assembled from private constant-return methods. */
final class PcovJitSchema
{
    /** Select one schema while retaining the private method's source location. */
    public function describe(bool $mutation): array
    {
        return $mutation ? $this->mutationSchema() : $this->banSchema();
    }

    /** Return the list schema. */
    private function banSchema(): array
    {
        return ['ban' => 'string'];
    }

    /** Return the mutation schema. */
    private function mutationSchema(): array
    {
        return ['success' => 'bool'];
    }

    /** Deliberately leave a method with the same return value uncalled. */
    private function unusedSchema(): array
    {
        return ['ban' => 'string'];
    }

    /** Call this method only while recording is stopped. */
    public function stoppedSchema(): array
    {
        return ['stopped' => 'bool'];
    }
}
