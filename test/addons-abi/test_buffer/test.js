'use strict';
// Flags: --expose-gc

var common = require('../../common');
var binding = require(`./build/${common.buildType}/test_buffer`);
var assert = require( "assert" );

assert( binding.newBuffer().toString() === binding.theText,
  "buffer returned by newBuffer() has wrong contents" );
assert( binding.newExternalBuffer().toString() === binding.theText,
  "buffer returned by newExternalBuffer() has wrong contents" );
console.log( "gc1" );
global.gc();
assert( binding.getDeleterCallCount(), 1, "deleter was not called" );
assert( binding.copyBuffer().toString() === binding.theText,
  "buffer returned by copyBuffer() has wrong contents" );

var buffer = binding.staticBuffer();
assert( binding.bufferHasInstance( buffer ), true, "buffer type checking fails" );
assert( binding.bufferInfo( buffer ), true, "buffer data is accurate" );
buffer = null;
global.gc();
console.log( "gc2" );
assert( binding.getDeleterCallCount(), 2, "deleter was not called" );
