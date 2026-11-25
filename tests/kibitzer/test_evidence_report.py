import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT / "tools/kibitzer/evidence_report.py"
spec=importlib.util.spec_from_file_location("kibitzer_evidence_report",SOURCE)
module=importlib.util.module_from_spec(spec)
assert spec.loader
sys.modules[spec.name]=module
spec.loader.exec_module(module)


def entry(version, choice, status, applicable=False,
          freq=None, agreement=None):
    return {"phase":"Preflop","cards":"K♣ T♥",
            "autoplay":{
                "policyVersion":version,"choice":choice,
                "reason":"synthetic-test",
                "preflopReference":{
                    "playersAlive":6,
                    "externalOpeningReference":{
                        "status":status,
                        "applicable":applicable,
                        "openingFrequency":freq,
                        "policyAgreement":agreement,
                        "referenceBB":5,
                        "referencePosition":"BTN",
                        "sourceId":"test-fixture"
                    }}}}


class ReportTest(unittest.TestCase):
    def test_coverage_and_deviations(self):
        v="kibitzer-autoplay-v5-shadow"
        data={"hands":[
            {"session":14,"hand":1,"entries":[
                entry(v,"FOLD","external-opening-chart-reference",True,1.0,
                      "diverges-from-strong-reference"),
                entry(v,"BET/RAISE","external-opening-chart-reference",True,.45,
                      "mixed-frequency-no-verdict"),
                entry(v,"FOLD","table-geometry-not-comparable"),
                entry("kibitzer-autoplay-v4","FOLD","not-logged")
            ]},
            {"session":13,"hand":2,"entries":[
                entry(v,"BET/RAISE","external-opening-chart-reference",True,1.0,
                      "agrees-with-strong-reference")
            ]}
        ]}
        report=module.summarize(data,session=14)
        self.assertEqual(report["selectedAutoplayDecisions"],3)
        self.assertEqual(report["comparableOpeningReferences"],2)
        self.assertEqual(report["applicabilityStatuses"][
            "table-geometry-not-comparable"],1)
        self.assertEqual(report["agreements"][
            "diverges-from-strong-reference"],1)
        self.assertEqual(report["agreements"]["mixed-frequency-no-verdict"],1)
        self.assertEqual(report["byGameContext"][0]["divergencesFromNearPure"],1)
        self.assertEqual(len(report["nearPureDivergenceExamples"]),1)
        self.assertEqual(report["nearPureDivergenceExamples"][0]["cards"],"K♣ T♥")
        report_all=module.summarize(data,version=None)
        self.assertEqual(report_all["selectedAutoplayDecisions"],5)
        self.assertEqual(report_all["comparableOpeningReferences"],3)

    def test_absent_reference_is_not_zero_mistakes(self):
        older={"hands":[{"session":13,"entries":[
            {"phase":"Preflop","autoplay":{
                "policyVersion":"kibitzer-autoplay-v2","choice":"FOLD"}}
        ]}]}
        report=module.summarize(older,version="kibitzer-autoplay-v2")
        self.assertEqual(report["selectedAutoplayDecisions"],1)
        self.assertEqual(report["comparableOpeningReferences"],0)
        self.assertEqual(report["applicabilityStatuses"],{"not-logged":1})
        self.assertEqual(report["agreements"],{})


if __name__=="__main__":
    unittest.main()
