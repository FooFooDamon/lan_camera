alias lanc_client='screen -d -m -U -S lanc_client -t lanc_client -h 100000 -L -Logfile /tmp/lanc_client.log bash -c "time bash /opt/lan_camera/scripts/lanc_client.sh"'
alias lanc_server='screen -d -m -U -S lanc_server -t lanc_server -h 100000 -L -Logfile /tmp/lanc_server.log bash -c "time bash /opt/lan_camera/scripts/lanc_server.sh"'

