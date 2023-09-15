

// Java-like hash code
export const hashCode = (inputString: string) => {
  let hash = 0;
  for (let i = 0; i < inputString.length; i++) {
      var code = inputString.charCodeAt(i);
      hash = ((hash<<5)-hash)+code;
      hash = hash & hash; // Convert to 32bit integer
  }
  return hash;
}