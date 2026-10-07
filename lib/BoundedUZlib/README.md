# BoundedUZlib

Project copy of the decoder and checksums from the pinned ArduinoUZlib
commit `20aff95cd80c141f80bdbf66895409a0046d2c2f`, derived from Joergen Ibsen's
tinf and Paul Sokolovsky's uzlib. Original copyright and license notices remain
in every applicable source file. This is an altered project copy.

Changes: public names use the `bounded_uzlib_` prefix; optional paranoid table
and end-of-block checks are enabled; literal and distance decoder error returns
and input exhaustion are rejected before using a symbol as a table index.
The existing third-party dependency is unchanged.

Only inflate, gzip header parsing and checksums are compiled. Input/allocation/
output limits, header CRC, data CRC, output size and complete consumption are
enforced by `include/bounded_gzip.h`. Main firmware and host tests compile this
same checked-in C source, rather than relying on an ignored dependency patch.
