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
        doc["config"].pop("tree_teardown_checksum", None)
        for test in doc["tests"]:
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


if __name__ == "__main__":
    unittest.main()
