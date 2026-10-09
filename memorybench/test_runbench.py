import copy
import contextlib
import io
import json
import unittest

import runbench


class ArenaMeasurementStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pinned = runbench.load_results(runbench.RESULTS_JSON)

    def results_without_arena(self):
        doc = copy.deepcopy(self.pinned)
        doc["config"]["addressing"] = "native"
        doc["config"]["scope_frames"] = True
        doc["config"].pop("tree_teardown_checksum", None)
        for test in doc["tests"]:
            if test["id"] in ("Test 1", "Test 3"):
                test["rows"].append({"label": "Zane (four-owner scope frames)",
                                     "samples_ns": [1000.0] * 20})
            if test["id"] in ("Test 8", "Test 10"):
                test["rows"] = [r for r in test["rows"]
                                if not r["label"].startswith("Arena")]
        return doc

    def rendered(self, doc):
        return json.loads(runbench.build_tests_json(doc, {}))

    def test_missing_arena_is_reported_without_a_zero_or_eliminated_row(self):
        doc = self.results_without_arena()
        rendered = self.rendered(doc)
        for index in (7, 9):
            test = rendered[index]
            self.assertTrue(any("Not measured" in m["val"] for m in test["meta"]))
            self.assertFalse(any(label.startswith("Arena") for label in test["labels"]))
            self.assertFalse(any(label.startswith("Arena") for label in test["eliminated"]))
            self.assertTrue(all(value > 0 for value in test["data"]))
        self.assertIn("Not measured", runbench.render_text(doc))

    def test_fresh_full_suite_clears_the_missing_measurement_status(self):
        doc = self.results_without_arena()
        doc["config"]["tree_teardown_checksum"] = True
        labels = {"Test 8": "Arena (bump + end-of-run reset)",
                  "Test 10": "Arena cascade visit + bulk reset"}
        for test in doc["tests"]:
            if test["id"] in labels:
                test["rows"].append({"label": labels[test["id"]],
                                     "samples_ns": [1000.0] * 20})
                self.assertEqual(runbench.measurement_notes(test, doc["config"]), [])
        for index in (7, 9):
            test = self.rendered(doc)[index]
            self.assertFalse(any(m["label"] == "Measurement status" for m in test["meta"]))
            arena = next(i for i, label in enumerate(test["labels"])
                         if label.startswith("Arena"))
            self.assertEqual(test["data"][arena], 1.0)
            self.assertEqual(test["colors"][arena], "#3aab76")

    def test_eliminated_arena_is_still_reported_as_unmeasured(self):
        """An eliminated required row must not satisfy the timing requirement."""
        doc = self.results_without_arena()
        doc["config"]["tree_teardown_checksum"] = True
        labels = {"Test 8": "Arena (bump + end-of-run reset)",
                  "Test 10": "Arena cascade visit + bulk reset"}
        for test in doc["tests"]:
            if test["id"] in labels:
                test["rows"].append({"label": labels[test["id"]], "eliminated": True})
                notes = runbench.measurement_notes(test, doc["config"])
                self.assertEqual(len(notes), 1)
                self.assertIn("Not measured", notes[0])
                self.assertIn(labels[test["id"]], notes[0])
        for index in (7, 9):
            rendered = self.rendered(doc)[index]
            self.assertTrue(any("Not measured" in m["val"] for m in rendered["meta"]))
            self.assertFalse(any(label.startswith("Arena") for label in rendered["labels"]))
            self.assertTrue(any(label.startswith("Arena") for label in rendered["eliminated"]))
        self.assertEqual(runbench.render_text(doc).count("Not measured"), 2)
        html = runbench.render_html(runbench.build_tests_json(doc, {}))
        self.assertEqual(html.count("Not measured"), 2)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            runbench.report(doc, {})
        self.assertEqual(output.getvalue().count("Not measured"), 2)

    def test_arena_only_patch_does_not_hide_an_old_teardown_protocol(self):
        doc = self.results_without_arena()
        tree = next(t for t in doc["tests"] if t["id"] == "Test 10")
        tree["rows"].append({"label": "Arena cascade visit + bulk reset",
                              "samples_ns": [1000.0] * 20})
        notes = runbench.measurement_notes(tree, doc["config"])
        self.assertEqual(len(notes), 1)
        self.assertIn("predate the checksum", notes[0])


class AddressingMeasurementStatusTests(unittest.TestCase):
    def test_pinned_timings_are_marked_as_the_earlier_model_without_changes(self):
        doc = runbench.load_results(runbench.RESULTS_JSON)
        original = copy.deepcopy(doc)
        for test in doc["tests"]:
            notes = runbench.measurement_notes(test, doc["config"])
            self.assertTrue(any("earlier segmented-offset model" in note for note in notes))
        self.assertIn("earlier segmented-offset model", runbench.render_text(doc))
        self.assertEqual(doc, original)

    def test_native_run_does_not_carry_the_old_addressing_warning(self):
        doc = runbench.load_results(runbench.RESULTS_JSON)
        doc["config"]["addressing"] = "native"
        for test in doc["tests"]:
            notes = runbench.measurement_notes(test, doc["config"])
            self.assertFalse(any("earlier segmented-offset model" in note for note in notes))


class ScopeFrameMeasurementStatusTests(unittest.TestCase):
    def test_native_per_owner_run_still_requires_scope_frame_measurements(self):
        doc = runbench.load_results(runbench.RESULTS_JSON)
        doc["config"]["addressing"] = "native"
        for test in doc["tests"]:
            if test["id"] in ("Test 1", "Test 3"):
                notes = runbench.measurement_notes(test, doc["config"])
                self.assertTrue(any("Not measured" in note for note in notes))
                self.assertTrue(any("predate per-scope" in note for note in notes))
        self.assertIn("predate per-scope", runbench.render_text(doc))

    def test_scope_marker_alone_cannot_supply_missing_measurements(self):
        doc = runbench.load_results(runbench.RESULTS_JSON)
        doc["config"].update(addressing="native", scope_frames=True)
        for test in doc["tests"]:
            if test["id"] in ("Test 1", "Test 3"):
                notes = runbench.measurement_notes(test, doc["config"])
                self.assertEqual(len(notes), 1)
                self.assertIn("Not measured", notes[0])

    def test_frame_rows_and_protocol_clear_the_status(self):
        doc = runbench.load_results(runbench.RESULTS_JSON)
        doc["config"].update(addressing="native", scope_frames=True)
        for test in doc["tests"]:
            if test["id"] in ("Test 1", "Test 3"):
                test["rows"].append({"label": "Zane (four-owner scope frames)",
                                     "samples_ns": [1000.0] * 20})
                self.assertEqual(runbench.measurement_notes(test, doc["config"]), [])


if __name__ == "__main__":
    unittest.main()
