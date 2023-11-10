const login_btn = document.getElementById("Login");

login_btn.addEventListener("click", async function() {
  try {
    const url = '/num'; // Add 'http://' before the URL
    const response = await fetch(url);
    const data = await response.json();
    console.log('Output from HTTP request:', data);
  } catch (error) {
    console.error('Error fetching data:', error);
  }
});
