set -e

# bash -x gce_virtio.sh hello hello ricarkol

NAME=$1
CANONICAL=$2
GCS_FOLDER=$3

APP=$NAME

mkdir -p tmp

#mirage configure -t virtio --dhcp=true --net=direct --network=tap0
#make clean
#make
bash ./unikernel-mkimage-debian.sh tmp/disk.raw tests/test_${APP}/test_${APP}.virtio
cd tmp/
tar -czvf ${NAME}.tar.gz disk.raw
cd ..

# Upload the file to Google Compute Storage as the original filename

gsutil cp tmp/${NAME}.tar.gz  gs://${GCS_FOLDER}/
# Copy/Alias it as *-latest
gsutil cp gs://${GCS_FOLDER}/${NAME}.tar.gz  gs://${GCS_FOLDER}/${CANONICAL}-latest.tar.gz

# Delete the image if it exists
yes | gcloud compute images delete ${CANONICAL}-latest | true

# Create an image from the new latest file
gcloud compute images create ${CANONICAL}-latest --source-uri  gs://${GCS_FOLDER}/${CANONICAL}-latest.tar.gz

# Boot an instance from said image
gcloud compute instances create --machine-type f1-micro --preemptible --image ${CANONICAL}-latest ${CANONICAL}
