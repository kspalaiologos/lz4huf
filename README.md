# lz4huf

An attempt to marry a fast Lempel-Ziv codec (LZ4) with a fast entropy coder (Huff0) to achieve better compression ratio with a negligible time overhead. Supports parallel compression.

Since most of the code in this repository was written by Yann Collet for the LZ4 and FSE (Huff0) projects, I do not take any credit for making this software.

## Does this work?

Kind of. lz4huf certainly retains respectable performance, but the compression ratio improvement is certainly not as good as you would perhaps expect it to be (usually oscillates near `zstd -1/-2`). The reason behind this is rather simple - the LZ4 minimum match length is 4 (hence the name!), so even though LZ4 on its own does not make use of any sort of entropy coding, its output is already quite dense:

```bash
% lz4 -12kq configure
% wc -c configure configure.lz4
455633 configure
119340 configure.lz4
% ent configure
Entropy = 5.283604 bits per byte.

Optimum compression would reduce the size
of this 455633 byte file by 33 percent.
[...]
% ent configure.lz4
Entropy = 7.087665 bits per byte.

Optimum compression would reduce the size
of this 119340 byte file by 11 percent.
% ./lz4huf configure configure.lz4h
% wc -c configure.lz4h
111875 configure.lz4h
[...]
% lz4 -12kq jquery-3.6.3.min.js
% ./lz4huf jquery-3.6.3.min.js jquery-3.6.3.min.js.lz4h
% gzip -9k jquery-3.6.3.min.js
% wc -c jquery*
 89947 jquery-3.6.3.min.js
 30935 jquery-3.6.3.min.js.gz
 38270 jquery-3.6.3.min.js.lz4
 34147 jquery-3.6.3.min.js.lz4h
193299 total
```

## Should I use this?

Probably not.
