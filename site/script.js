async function send_http_request() {
    try {
      const url = '127.0.0.1:8082/num'; // Replace with your desired URL
      const response = await fetch(url);
      const data = await response.json();
      console.log('Output from HTTP request:', data);
    } catch (error) {
      console.error('Error fetching data:', error);
    }
  }