use libp2p::{
  core::{muxing::StreamMuxerBox, transport::OrTransport, upgrade},
  gossipsub, identity, mdns, noise,
  swarm::NetworkBehaviour,
  swarm::{SwarmBuilder, SwarmEvent},
  tcp, yamux, PeerId, Transport,
};
use futures::{future::Either, prelude::*, select};

pub fn testing() {
  // Create a random PeerId
  let id_keys = identity::Keypair::generate_ed25519();
  let local_peer_id = PeerId::from(id_keys.public());
  println!("Local peer id: {local_peer_id}");

  // Set up an encrypted DNS-enabled TCP Transport over the Mplex protocol.
  let tcp_transport = tcp::async_io::Transport::new(tcp::Config::default().nodelay(true))
    .upgrade(upgrade::Version::V1)
    .authenticate(
      noise::NoiseAuthenticated::xx(&id_keys).expect("signing libp2p-noise static keypair"),
    )
    .multiplex(yamux::YamuxConfig::default())
    .timeout(std::time::Duration::from_secs(20))
    .boxed();
  // let quic_transport = quic::async_std::Transport::new(quic::Config::new(&id_keys));
  // let transport = OrTransport::new(quic_transport, tcp_transport)
  //   .map(|either_output, _| match either_output {
  //     Either::Left((peer_id, muxer)) => (peer_id, StreamMuxerBox::new(muxer)),
  //     Either::Right((peer_id, muxer)) => (peer_id, StreamMuxerBox::new(muxer)),
  //   })
  //   .boxed();
}
