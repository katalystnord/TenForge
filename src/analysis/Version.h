#pragma once

// tf_analysis: the standards engine. This header and every header beside it
// must compile with no Qt and no VTK on the include path; the test
// `analysis_headers_compile_without_qt` builds them that way.

namespace tf::analysis {

// The engine's version, which results files record beside the application's
// (docs/specs/results-v1.md), so a result can be traced to the exact engine
// that computed it.
const char *version();

} // namespace tf::analysis
