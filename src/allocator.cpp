#include "allocator.h"

/*
 * This file includes default implementations of the Allocator virtual
 * functions. This is needed because we do hot reload, which changes the
 * addresses of things in the code segment, so vtable pointers and function
 * pointers become invalidated. Therefore this file and the corresponding
 * function implementations are compiled into the main executable and never
 * reloaded.
 */

void Allocator::impl_arenaPushDestructor(DestructorBase &entry) NOEXCEPT {}

void *Allocator::impl_arenaNewScope() NOEXCEPT { return nullptr; }

void Allocator::impl_arenaRestoreScope(void *handle) NOEXCEPT {}
