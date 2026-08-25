# GDDM shapes and colours example

`GDDMALL.RPG` is a fixed-format RPG/400 program for an IBM i GDDM graphics
device. It exercises the primitives that 5250ng currently decodes:

- colour changes and double-width lines;
- a multicolour frame and diagonals;
- patterned filled triangles and rectangles (`GSAREA`/`GSENDA`);
- marker symbols 0 through 8 (`GSMS`/`GSMARK`); and
- a circular outline (`GSARC`), which GDDM converts to device geometry.

The program uses the normal GDDM lifecycle (`FSINIT`, drawing calls, `ASREAD`,
and `FSTERM`) and the default 0..100 world-coordinate window. Coordinates are
therefore independent of the 5292's 480x288 pels; GDDM performs the mapping.

## Install and run on IBM i

The target library and source file can be created once with:

```text
CRTLIB LIB(GDDMTEST) TYPE(*TEST) TEXT('GDDM examples')
CRTSRCPF FILE(GDDMTEST/QRPGSRC) RCDLEN(112) TEXT('GDDM examples')
ADDPFM FILE(GDDMTEST/QRPGSRC) MBR(GDDMALL) TEXT('GDDM shapes and colours')
```

Create member `GDDMALL` in `GDDMTEST/QRPGSRC` and copy the contents of
`GDDMALL.RPG` into it. This is fixed-format RPG/400: preserve the leading
columns and the `C`/`I` form-type positions when transferring the source.
Compile and run it with:

```text
CRTRPGPGM PGM(GDDMTEST/GDDMALL) SRCFILE(GDDMTEST/QRPGSRC) REPLACE(*YES)
CALL GDDMTEST/GDDMALL
```

Use a graphics-capable 5292 session (`IBM-5292-2`) to see the graphics plane.
On a non-graphics 5250 session, IBM i uses its dummy `5292M2` device and the
program still completes, but no graphics block is displayed.

## Expected layout

The upper-left area contains the red frame and blue/magenta diagonals. The
right half contains a green patterned triangle and a cyan patterned rectangle.
The marker row runs across the middle of the screen in yellow. A white circle
is drawn near the lower-left area. `ASREAD` flushes the retained page and waits
for an AID key before `FSTERM` releases the graphics state.
