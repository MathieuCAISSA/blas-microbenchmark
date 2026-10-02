/* Unit tests for the page's logic: the functions report.html exposes as
 * window.bmbReport, fed small hand-built results so that each decision of
 * #1 is pinned by a check that names it.
 *
 * test_bmb_report_js.sh inserts this file after the page's own script in a
 * real report and runs it in a headless browser. The outcome is written
 * into a pre element, id bmb-test-results, one "ok"/"FAIL" line per check,
 * which the shell script reads back from the DOM. (Not spelt as a tag here:
 * this source is in the DOM too, and the script would find it first.)
 *
 * Plain ES5, like the page, and never the two characters that would end
 * the <script> element this is pasted into: the harness refuses them. */

(function () {
  "use strict";

  var lines = [];
  var failures = 0;
  var R = window.bmbReport;

  function check(ok, what) {
    lines.push((ok ? "ok   " : "FAIL ") + what);
    if (!ok) { failures++; }
  }

  function eq(got, want, what) {
    var g = JSON.stringify(got), w = JSON.stringify(want);
    check(g === w, g === w ? what : what + ": got " + g + ", want " + w);
  }

  function near(got, want, what) {
    var ok = typeof got === "number" && Math.abs(got - want) <= 1e-9 * Math.max(1, Math.abs(want));
    check(ok, ok ? what : what + ": got " + got + ", want " + want);
  }

  function test(name, fn) {
    try {
      fn();
    } catch (e) {
      check(false, name + ": threw " + e);
    }
  }

  /* One benchmark's JSON, as the C side writes it. rows: [threads, dim1,
   * dim2 or null, time_s, gflops or null, gbytes_per_s or null]. */
  function result(o) {
    var r = {
      version: "1.0.0", backend: o.backend || "openblas",
      blas: o.blas === undefined ? "OpenBLAS 0.3.26 Haswell" : o.blas,
      machine: { cpu: o.cpu === undefined ? "Test CPU" : o.cpu, caches: o.caches || [] },
      routine: o.routine || "ddot",
      dim1_label: o.dim1Label || "Vector size",
      results: (o.rows || []).map(function (x) {
        var row = { thread_count: x[0], dim1: x[1], time_s: x[3] };
        if (x[2] !== null && x[2] !== undefined) { row.dim2 = x[2]; }
        if (x[4] !== null && x[4] !== undefined) { row.gflops = x[4]; }
        if (x[5] !== null && x[5] !== undefined) { row.gbytes_per_s = x[5]; }
        return row;
      })
    };
    if (o.dim2Label) { r.dim2_label = o.dim2Label; }
    if (o.label) { r.label = o.label; }
    return r;
  }

  function entry(file, o) { return { file: file, result: result(o) }; }

  var GEMM = { routine: "dgemm", dim1Label: "Matrix dim1 (M=K)", dim2Label: "Matrix dim2 (N)" };
  var GEMV = { routine: "dgemv", dim1Label: "Matrix dim1 (M)", dim2Label: "Matrix dim2 (N)" };

  function gemm(o) {
    var k;
    for (k in GEMM) { if (o[k] === undefined) { o[k] = GEMM[k]; } }
    return o;
  }

  function gemv(o) {
    var k;
    for (k in GEMV) { if (o[k] === undefined) { o[k] = GEMV[k]; } }
    return o;
  }

  function names(ids) { return ids.map(function (id) { return id.name; }); }
  function routine(model, name) { return model.routines.filter(function (rt) { return rt.name === name; })[0]; }
  function byName(model, name) { return model.identities.filter(function (id) { return id.name === name; })[0]; }

  if (!R) {
    check(false, "the page exposes window.bmbReport");
  } else {

    /* ---- Decision 1: what a series is, and merging ---- */

    test("merge", function () {
      var m = R.buildModel([
        entry("a.json", { rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("b.json", { rows: [[1, 16, null, 1e-6, 2, 16]] })
      ]);
      eq(m.identities.length, 1, "decision 1: two files with the same backend, BLAS, CPU and label are one series");
      eq(R.pointsOf(m.routines[0].seriesOrder[0]).map(function (p) { return p.d1; }).sort(function (a, b) { return a - b; }), [8, 16],
         "decision 1: the merged series has the points of both files");
      eq(m.identities[0].files, ["a.json", "b.json"], "decision 1: the series remembers both files");
    });

    test("identity", function () {
      eq(R.buildModel([
        entry("a.json", { label: "a", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("b.json", { label: "b", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("c.json", { blas: "OpenBLAS 0.3.28 Zen", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("d.json", { cpu: "Other CPU", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("e.json", { backend: "blis", blas: "0.9.0", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]).identities.length, 5, "decision 1: a different label, BLAS version, CPU or backend is a different series");
    });

    test("duplicates", function () {
      var m = R.buildModel([
        entry("slow.json", { rows: [[1, 8, null, 2e-6, 1, 8]] }),
        entry("fast.json", { rows: [[1, 8, null, 1e-6, 2, 16]] }),
        entry("slower.json", { rows: [[1, 8, null, 3e-6, 0.5, 4]] })
      ]);
      var rt = m.routines[0];
      var pts = R.pointsOf(rt.seriesOrder[0]);
      eq(pts.length, 1, "decision 1: a point measured in three files is one point");
      eq(pts[0].file, "fast.json", "decision 1: the fastest measurement is kept, whatever the file order");
      eq(rt.duplicates.length, 2, "decision 1: each duplicate is recorded for the page to report");
      eq(rt.duplicates[0].other, "slow.json", "decision 1: a duplicate names the file that lost");
      near(rt.duplicates[0].gap, 100, "decision 1: the gap is how much slower the other file was, in %");
    });

    test("malformed", function () {
      var m = R.buildModel([
        { file: "empty.json", result: {} },
        null,
        entry("ok.json", { rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      eq(m.problems, ["empty.json could not be read: it is not a benchmark result",
                      "result 2 could not be read: it is not a benchmark result"],
         "a result that is not one is reported by name");
      eq(m.routines.length, 1, "a bad result does not take the good ones down with it");
    });

    test("names", function () {
      var m = R.buildModel([
        entry("a.json", { label: "a", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("b.json", { label: "b", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("c.json", { backend: "blis", blas: "0.9.0", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      eq(names(m.identities), ["openblas · a", "openblas · b", "blis"],
         "legend names: the backend alone when it is unique, plus only what tells same-backend series apart");

      m = R.buildModel([
        entry("a.json", { blas: "OpenBLAS 0.3.26 Haswell MAX_THREADS=64", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("b.json", { blas: "OpenBLAS 0.3.28 Haswell MAX_THREADS=64", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      eq(names(m.identities), ["openblas · OpenBLAS 0.3.26", "openblas · OpenBLAS 0.3.28"],
         "legend names: the BLAS version shortened to its first two words");

      m = R.buildModel([
        entry("a.json", { cpu: "implementer 0x41, part 0xd49", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("b.json", { cpu: "Intel Thing", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      eq(names(m.identities), ["openblas · Arm Neoverse-N2", "openblas · Intel Thing"],
         "legend names: the CPU, aarch64 codes named for display");
    });

    /* ---- Decision 2: colours and the reference ---- */

    test("slots", function () {
      var m = R.buildModel([
        entry("1.json", { backend: "blis", blas: "", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("2.json", { backend: "armpl", blas: "", rows: [[1, 8, null, 1e-6, 1, 8]] }),
        entry("3.json", { backend: "openblas", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      eq(names(m.identities), ["openblas", "armpl", "blis"],
         "decision 2: OpenBLAS first, then by name, never by file order");
      eq(m.identities.map(function (id) { return id.slot; }), [1, 2, 3], "colour slots follow that order");

      var many = [];
      for (var i = 1; i <= 9; i++) { many.push(entry(i + ".json", { label: "l" + i, rows: [[1, 8, null, 1e-6, 1, 8]] })); }
      m = R.buildModel(many);
      eq(m.identities.map(function (id) { return id.slot; }), [1, 2, 3, 4, 5, 6, 7, 8, 0],
         "a ninth series gets no colour rather than a made-up one");
    });

    test("reference", function () {
      var m = R.buildModel([
        entry("1.json", gemm({ backend: "blis", blas: "", rows: [[1, 8, 8, 1e-6, 1, 8]] })),
        entry("2.json", gemm({ backend: "openblas", rows: [[1, 8, 8, 1e-6, 1, 8]] })),
        entry("3.json", { backend: "netlib", blas: "", rows: [[1, 8, null, 1e-6, 1, 8]] })
      ]);
      var cand = R.comparable(m.identities, m.routines);
      eq(names(cand), ["openblas", "blis"],
         "decision 2: only series measured next to another can be the reference; OpenBLAS is the default");
      var blis = byName(m, "blis");
      eq(R.referenceFor(routine(m, "dgemm"), blis).name, "blis", "decision 2: the chosen reference is used where it was measured");
      eq(R.referenceFor(routine(m, "ddot"), blis).name, "netlib", "decision 2: elsewhere the routine's own first series is");
      eq(R.referenceFor(routine(m, "dgemm"), null).name, "openblas", "decision 2: no choice means OpenBLAS");
    });

    test("ratios", function () {
      var m = R.buildModel([
        entry("o.json", gemm({ rows: [[1, 64, 64, 1, 10, null], [1, 128, 128, 1, 20, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 64, 64, 1, 20, null], [1, 128, 128, 1, 10, null]] })),
        entry("n.json", gemm({ backend: "netlib", blas: "", rows: [[1, 256, 256, 1, 1, null]] }))
      ]);
      var rt = routine(m, "dgemm");
      var sd = R.sizeData(rt, 1);
      var rd = R.ratioData(rt, sd.list, sd.slice, 1, byName(m, "openblas"));
      eq(rd.series.map(function (x) { return x.identity.name; }), ["blis", "netlib"], "decision 2: one ratio curve per other series");
      eq(rd.series[0].points, [{ x: 64, y: 2 }, { x: 128, y: 0.5 }], "decision 2: each ratio is series over reference, point by point");
      eq(rd.worst, 2, "the worst ratio counts 2x slower like 2x faster");
      eq(rd.notes, ["netlib has no point in common with openblas at 1 thread, so it is not in the comparison chart."],
         "a series with nothing in common with the reference is a note, not an empty curve");
      eq(R.ratioData(rt, sd.list, sd.slice, 1, byName(m, "nonexistent")), null, "no ratio chart without the reference");
    });

    test("ratio axis", function () {
      eq(R.ratioDomain(1), [0.8, 1.25], "decision 2: the ratio axis spans at least x0.8 to x1.25");
      near(R.ratioDomain(2)[0] * R.ratioDomain(2)[1], 1, "decision 2: the log axis is centred on x1");
      eq(R.ratioTicks([0.8, 1.25]), [0.8, 1, 1.25], "a short ratio axis keeps x0.8 and x1.25");
      eq(R.ratioTicks(R.ratioDomain(20)), [0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20], "a long one drops them");
    });

    test("thinTicks", function () {
      function decade(px) { return function (v) { return -px * Math.log(v) / Math.LN10; }; }
      eq(R.thinTicks([0.8, 1, 1.25], decade(100), 16), [1], "log ticks closer than 16px are dropped, x1 kept");
      eq(R.thinTicks([0.8, 1, 1.25], decade(1000), 16), [0.8, 1, 1.25], "log ticks far enough apart all stay");
      eq(R.thinTicks([0.5, 0.8, 1, 1.25, 2], decade(100), 16), [0.5, 1, 2], "the ones nearest x1 crowd out first");
    });

    /* ---- Decision 3: threads and slicing ---- */

    test("default threads", function () {
      var m = R.buildModel([
        entry("o.json", gemm({ rows: [[1, 8, 8, 1, 1, null], [2, 8, 8, 1, 1, null], [4, 8, 8, 1, 1, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 8, 8, 1, 1, null], [2, 8, 8, 1, 1, null]] }))
      ]);
      eq(R.defaultThreads(routine(m, "dgemm")), { all: [1, 2, 4], shared: [1, 2], initial: 2 },
         "decision 3: the default is the highest thread count every series has");
      m = R.buildModel([
        entry("o.json", gemm({ rows: [[4, 8, 8, 1, 1, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 8, 8, 1, 1, null]] }))
      ]);
      eq(R.defaultThreads(routine(m, "dgemm")).initial, 4, "decision 3: none shared, the highest overall");
    });

    test("slices", function () {
      var m = R.buildModel([
        entry("d.json", { rows: [[1, 8, null, 1, 1, 1], [1, 16, null, 1, 1, 1]] }),
        entry("sq.json", gemv({ rows: [[1, 8, 8, 1, 1, 1], [1, 16, 16, 1, 1, 1]] }))
      ]);
      var s = R.sliceForSize(routine(m, "ddot"), 1);
      eq([s.label, s.note, s.pick({ d1: 8, d2: null })], ["Vector size", undefined, 8], "level 1: plotted against its one size");

      s = R.sliceForSize(routine(m, "dgemv"), 1);
      eq([s.label, s.note], ["Size (M=N)", "square matrices"], "decision 3: without -M, plotted against the square size");

      m = R.buildModel([entry("g.json", gemv({ rows: [[1, 8, 8, 1, 1, 1], [1, 8, 16, 1, 1, 1], [1, 16, 8, 1, 1, 1], [1, 16, 16, 1, 1, 1]] }))]);
      s = R.sliceForSize(m.routines[0], 1);
      eq([s.keep({ d1: 16, d2: 16 }), s.keep({ d1: 8, d2: 16 })], [true, false], "decision 3: a grid is sliced to its squares");
      check(/heatmap/.test(s.note), "decision 3: and the slice says where the other shapes went");

      m = R.buildModel([entry("f.json", gemv({ rows: [[1, 8, 100, 1, 1, 1], [1, 16, 100, 1, 1, 1]] }))]);
      s = R.sliceForSize(m.routines[0], 1);
      eq([s.label, s.note, s.pick({ d1: 16, d2: 100 })], ["Matrix dim1 (M)", "Matrix dim2 (N) = 100", 16],
         "a fixed second size is plotted against the first, and named");
      m = R.buildModel([entry("f.json", gemv({ rows: [[1, 100, 8, 1, 1, 1], [1, 100, 16, 1, 1, 1]] }))]);
      s = R.sliceForSize(m.routines[0], 1);
      eq([s.label, s.pick({ d1: 100, d2: 16 })], ["Matrix dim2 (N)", 16], "a fixed first size is plotted against the second");
    });

    test("sizeData", function () {
      var m = R.buildModel([entry("g.json", gemv({ rows: [[1, 8, 8, 1, 1, 1], [1, 8, 16, 1, 1, 1], [2, 16, 16, 1, 1, 1], [1, 16, 16, 1, 1, 1]] }))]);
      var rt = m.routines[0];
      var sd = R.sizeData(rt, 1);
      eq(sd.xs, [8, 16], "the size chart has the slice's points at the chosen thread count");
      eq(sd.list[0].points.length, 2, "and only those");
      eq(Object.keys(rt.seriesOrder[0]).sort(), ["identity", "points"], "computing a chart leaves the series alone");
    });

    test("threadData", function () {
      var m = R.buildModel([
        entry("o.json", gemm({ rows: [[1, 64, 64, 1, 10, null], [2, 64, 64, 1, 15, null], [1, 128, 128, 1, 10, null], [2, 128, 128, 1, 20, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 64, 64, 1, 10, null], [2, 64, 64, 1, 20, null], [1, 128, 128, 1, 10, null]] }))
      ]);
      var td = R.threadData(routine(m, "dgemm"));
      eq(td.chosen, { d1: 64, d2: 64 }, "thread scaling: at the largest size every series measured at all its thread counts");
      near(td.series[0].points[1].efficiency, 0.75, "thread scaling: efficiency against ideal linear scaling");
      eq(td.series[0].ideal, [{ x: 1, y: 10 }, { x: 2, y: 20 }], "thread scaling: ideal is linear from the lowest count");

      m = R.buildModel([
        entry("o.json", gemm({ rows: [[1, 64, 64, 1, 10, null], [2, 64, 64, 1, 15, null], [1, 128, 128, 1, 10, null], [2, 128, 128, 1, 20, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 256, 256, 1, 10, null], [2, 256, 256, 1, 20, null]] }))
      ]);
      td = R.threadData(routine(m, "dgemm"));
      eq([td.chosen, td.series[0].size, td.series[1].size], [null, { d1: 128, d2: 128 }, { d1: 256, d2: 256 }],
         "thread scaling: no common size, each series at its own largest");

      m = R.buildModel([
        entry("o.json", gemm({ rows: [[1, 64, 64, 1, 10, null], [2, 64, 64, 1, 15, null]] })),
        entry("b.json", gemm({ backend: "blis", blas: "", rows: [[1, 64, 64, 1, 10, null], [2, 128, 128, 1, 20, null]] }))
      ]);
      eq(R.threadData(routine(m, "dgemm")).notes,
         ["blis has no size measured at every one of its thread counts, so it is not in the thread-scaling chart."],
         "thread scaling: a series with no complete size is a note");

      m = R.buildModel([entry("o.json", gemm({ rows: [[1, 64, 64, 1, 10, null]] }))]);
      eq(R.threadData(routine(m, "dgemm")), null, "no thread-scaling chart from one thread count");
    });

    /* ---- The heatmap ---- */

    test("heatmap", function () {
      var grid = [[1, 8, 8, 1, 1, null], [1, 8, 16, 1, 2, null], [1, 16, 8, 1, 3, null], [1, 16, 16, 1, 4, null]];
      var m = R.buildModel([entry("g.json", gemv({ rows: grid.concat([[2, 8, 8, 1, 1, null], [2, 16, 16, 1, 1, null]]) }))]);
      var rt = m.routines[0];
      var hd = R.heatData(rt, 1);
      eq([hd.d1s, hd.d2s, hd.vmax], [[8, 16], [8, 16], 4], "heatmap: both axes and one colour scale for every map");
      eq(R.heatData(rt, 2), null, "heatmap: the -M diagonal alone is not a grid");
      eq(R.threadsWithGrid(rt), [1], "heatmap: the page can say which thread count has the grid");
      eq([R.heatStep(0, 4), R.heatStep(2, 4), R.heatStep(4, 4)], [1, 4, R.STEPS], "heatmap: steps from 1 to the top of the ramp");

      m = R.buildModel([entry("d.json", { rows: [[1, 8, null, 1, 1, 1], [1, 16, null, 1, 1, 1]] })]);
      eq(R.heatData(m.routines[0], 1), null, "heatmap: none for a routine with one size");
    });

    /* ---- Cache markers ---- */

    test("cacheMarkers", function () {
      var caches = [{ level: 1, type: "data", size_bytes: 32768 },
                    { level: 1, type: "instruction", size_bytes: 32768 },
                    { level: 2, type: "unified", size_bytes: 1048576 }];
      /* Working set = GB/s x 1e9 x time: 16K, 64K, 256K bytes. */
      var rows = [[1, 1024, null, 1e-6, 1, 16.384], [1, 4096, null, 1e-6, 1, 65.536], [1, 16384, null, 1e-6, 1, 262.144]];
      var m = R.buildModel([entry("d.json", { caches: caches, rows: rows })]);
      var sd = R.sizeData(m.routines[0], 1);
      var mk = R.cacheMarkers(sd.list, sd.slice);
      eq(mk.length, 1, "cache markers: only where the sweep crosses a data cache; L1i and a cache past the end are not drawn");
      eq(mk[0].label, "L1d 32K", "cache markers: named by level, type and size");
      near(mk[0].x, 2048, "cache markers: placed by log interpolation of the working set");

      m = R.buildModel([
        entry("a.json", { caches: caches, rows: rows }),
        entry("b.json", { cpu: "Other", caches: [{ level: 1, type: "data", size_bytes: 49152 }], rows: rows })
      ]);
      sd = R.sizeData(m.routines[0], 1);
      eq(R.cacheMarkers(sd.list, sd.slice), [], "cache markers: none when the series ran on different caches");
    });

    /* ---- Formatting ---- */

    test("formatting", function () {
      eq(R.cpuName("implementer 0x41, part 0xd49"), "Arm Neoverse-N2", "decision 4: known aarch64 codes are named for display");
      eq(R.cpuName("implementer 0x41, part 0xfff"), "implementer 0x41, part 0xfff", "decision 4: unknown codes are shown raw");
      eq(R.cpuName(""), "unknown CPU", "an unknown CPU says so");
      eq([R.plural(1, "thread"), R.plural(2, "thread"), R.plural(2, "series")], ["1 thread", "2 threads", "2 series"], "plurals");
      eq([R.fmtSize(1000), R.fmtSize(16384), R.fmtSize(2097152)], ["1,000", "16K", "2M"], "sizes in binary suffixes when exact");
      eq([R.fmtBytes(49152), R.fmtBytes(12582912)], ["48K", "12M"], "cache sizes");
      eq([R.fmtRate(123.4), R.fmtRate(12.34), R.fmtRate(1.234)], ["123", "12.3", "1.23"], "rates to three figures");
      eq([R.fmtTime(1.5e-8), R.fmtTime(2e-5), R.fmtTime(0.25)], ["15.0 ns", "20.00 µs", "250.00 ms"], "times");

      var m = R.buildModel([entry("c.json", { routine: "dcopy", rows: [[1, 8, null, 1, null, 8]] })]);
      eq(R.metricField(m.routines[0]), "gbs", "dcopy, which does no arithmetic, is judged by GB/s");
    });
  }

  var summary = failures ? failures + " check(s) failed" : "all " + lines.length + " checks passed";
  var pre = document.createElement("pre");
  pre.id = "bmb-test-results";
  pre.textContent = lines.join("\n") + "\n" + summary + "\n";
  document.body.appendChild(pre);
})();
