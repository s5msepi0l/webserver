const login_btn = document.getElementById("Login");

login_btn.addEventListener("click", async function() {
  try {
    const response = await fetch("/num_post", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },

      body: "[64]"
    });

    
    const data = await response.json();
    console.log('Output from HTTP request:', data);
  } catch (error) {
    console.error('Error fetching data:', error);
  }
});
