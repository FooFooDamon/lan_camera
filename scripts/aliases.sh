alias lanc_client='screen -d -m -U -S lanc_client -t lanc_client -h 100000 -L -Logfile /tmp/lanc_client.log bash -c "time bash /opt/lan_camera/scripts/lanc_client.sh"'
alias lanc_server='screen -d -m -U -S lanc_server -t lanc_server -h 100000 -L -Logfile /tmp/lanc_server.log bash -c "time bash /opt/lan_camera/scripts/lanc_server.sh"'
alias lanc_client.sh='time bash /opt/lan_camera/scripts/lanc_client.sh'
alias lanc_server.sh='time bash /opt/lan_camera/scripts/lanc_server.sh'
alias test_lanc_client.sh='time bash /opt/lan_camera/scripts/test_lanc_client.sh'
alias test_lanc_server.sh='time bash /opt/lan_camera/scripts/test_lanc_server.sh'
alias renice_lanc_server='bash /opt/lan_camera/scripts/renice_lanc_threads.sh ~/etc/lan_camera.srv.json'
alias renice_lanc_client='bash /opt/lan_camera/scripts/renice_lanc_threads.sh ~/etc/lan_camera.cli.json'

