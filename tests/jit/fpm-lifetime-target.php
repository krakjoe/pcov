<?php

/** Supply closures whose runtime op_arrays are temporary copies of cached code. */
function pcov_lifetime_factory(int $offset): Closure
{
    return static function (int $value) use ($offset): int {
        return $value + $offset; // covered
    };
}

trait PcovLifetimeTrait
{
    /** Exercise method metadata copied while binding a trait to a class. */
    public function increment(int $value): int
    {
        return $value + 1; // covered
    }
}

/** Invoke a closure from a hot caller so the tracing JIT can inline its frame. */
function pcov_lifetime_call(Closure $callback, int $value): int
{
    return $callback($value); // covered
}

/** Leave an executable line uncalled to detect spurious coverage. */
function pcov_lifetime_unused(): int
{
    return 42; // uncovered
}
