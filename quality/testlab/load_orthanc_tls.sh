#!/bin/sh
# Loads the synthetic corpus into the test PACS with TLS, and checks that it asks for a trusted
# certificate. Run it with: docker compose --profile tls run --rm tools sh /lab/load_orthanc_tls.sh
set -e

BASE=https://orthanc-tls:8042

if curl -s -o /dev/null -u qa:qa-only --cacert /lab/tls/other-ca.pem "$BASE/system"
then
    echo "ERROR: HTTPS accepted a CA that did not sign its certificate"
    exit 1
fi

find /lab/corpus/synthetic -type f -name '*.dcm' | while read -r FILE
do
    curl -s -f -o /dev/null -u qa:qa-only --cacert /lab/tls/ca.pem -H 'Content-Type: application/dicom' --data-binary "@$FILE" "$BASE/instances" || echo "Rejected (the corpus has some invalid files on purpose): $FILE"
done

curl -s -f -u qa:qa-only --cacert /lab/tls/ca.pem "$BASE/statistics"
