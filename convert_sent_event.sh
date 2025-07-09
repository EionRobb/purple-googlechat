
[ -z "$1" ] && echo "Usage:  $0 OiQ4Y2NjY2IzMS1lYWZhLTRkZGQtOGZhNC02MjQzZWU0ZjgxYjI=" && exit

protoc -o googlechat.proto.desc googlechat.proto
echo "$1" | base64 -d - | protoc --decode StreamEventsRequest googlechat.proto

