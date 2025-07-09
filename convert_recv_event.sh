
[ -z "$1" ] && echo "Usage:  $0 CiEqFwoVMTAzODQ3NzgyMzk1NzY3ODIzMTg3QgZgDmoCCAASJDhjYWNkMTQxLTRjMDAtNDVmZi1iZDMwLTExODgzODA4OWIzOQ==" && exit

protoc -o googlechat.proto.desc googlechat.proto
echo "$1" | base64 -d - | protoc --decode StreamEventsResponse googlechat.proto

